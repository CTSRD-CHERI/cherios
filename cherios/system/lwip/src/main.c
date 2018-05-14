/*-
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sockets.h>
#include "cheric.h"
#include "virtio_queue.h"
#include "virtio_net.h"
#include "virtio.h"
#include "sockets.h"
#include "stdlib.h"
#include "malta_virtio_mmio.h"
#include "assert.h"
#include "mman.h"
#include "misc.h"
#include "syscalls.h"

#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/init.h"
#include "lwip/stats.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip4_frag.h"
#include "lwip/apps/httpd.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"

// This driver is a stepping stone to get lwip working. It will eventually be extracted into into the stand-alone driver
// and they will communicate via the socket API.

#define QUEUE_SIZE 0x10
#define CHERIOS_NET_MASK    "255.255.255.0"
#define CHERIOS_IP          "128.232.18.56"
#define CHERIOS_GATEWAY     "128.232.18.1"
#define CHERIOS_MAC         {0x00,0x16,0x3E,0xE8,0x12,0x38}
#define CHERIOS_HOST        "cherios"

typedef struct net_session {
    virtio_mmio_map* mmio;
    struct netif* nif;
    uint8_t irq;
    struct virtq virtq_send, virtq_recv;

    // Net headers for send and recieve (SEND_HDR_START for recv, second lot for send)
#define NET_HDR_P(N) session->net_hdrs_paddr + (sizeof(struct virtio_net_hdr) * (N))
#define SEND_HDR_START (QUEUE_SIZE)

    struct virtio_net_hdr* net_hdrs;
    size_t net_hdrs_paddr;
    struct pbuf* pbuf_recv_map[QUEUE_SIZE];
    struct pbuf* pbuf_send_map[QUEUE_SIZE];

    le16 recvs_free;
    le16 free_head_send;
    le16 free_head_recv;
    struct virtio_net_config config;

    ip4_addr_t gw_addr, my_ip, netmask;
    uint8_t mac[6];

} net_session;

struct tcp_con_args {
    const ip_addr_t addr;
    uint16_t port;
    err_t result;
};

typedef struct tcp_session {
    struct tcp_pcb* tcp_pcb;
    uni_dir_socket_fulfiller tcp_input_pushee;
    struct requester_32 tcp_output_pusher;
    struct tcp_session* next, *prev;
    struct tcp_con_args* con;
    enum poll_events events;
    uint8_t connected;

    uint64_t byte_mark;
    uint64_t byte_highest_seen;
    uint64_t byte_missing;
} tcp_session;

tcp_session* tcp_head = NULL;

#define FOR_EACH_TCP(T) for(tcp_session* T = tcp_head; T != NULL; T = T->next)

tcp_session* alloc_tcp_session(uni_dir_socket_fulfiller* tcp_output_pushee, uni_dir_socket_requester* tcp_input_pusher) {

    // Allocate and add to chain
    tcp_session* new = (tcp_session*)malloc(sizeof(tcp_session));
    new->next = tcp_head;
    new->prev = NULL;
    if(tcp_head)tcp_head->prev = new;
    tcp_head = new;

    // Init
    socket_internal_fulfiller_init(&new->tcp_input_pushee, SOCK_TYPE_PUSH);
    socket_internal_requester_init(&new->tcp_output_pusher.r, 32, SOCK_TYPE_PUSH, NULL);

    // Connect
    socket_internal_requester_connect(&new->tcp_output_pusher.r);
    socket_internal_fulfiller_connect(&new->tcp_input_pushee, tcp_input_pusher);

    return new;
}

void free_tcp_session(tcp_session* tcp_session) {
    // TODO dont wait here. Instead delay closing until we have pushed everything up

    // Close
    socket_internal_close_fulfiller(&tcp_session->tcp_input_pushee, 1, 0);
    socket_internal_close_requester(&tcp_session->tcp_output_pusher.r, 1, 0);

    // Free from close
    if(tcp_session->prev) tcp_session->prev->next = tcp_session->next;
    else tcp_head = tcp_session->next; // If we have no previous we are the head
    if(tcp_session->next) tcp_session->next->prev = tcp_session->prev;

    return;
}

err_t netsession_init(struct netif* nif) {
    assert(nif != NULL);

    // First init

    net_session* session = nif->state;
    session->nif = nif;

    // Setup queues
    cap_pair pair;
#define GET_A_PAGE (rescap_take(mem_request(0, MEM_REQUEST_MIN_REQUEST, NONE, own_mop).val, &pair), pair.data)

    assert(is_power_2(QUEUE_SIZE));

    session->virtq_send.num = QUEUE_SIZE;
    session->virtq_send.avail = (struct virtq_avail*)GET_A_PAGE;
    session->virtq_send.desc = (struct virtq_desc*)GET_A_PAGE;
    session->virtq_send.used = (struct virtq_used*)GET_A_PAGE;

    session->virtq_recv.num = QUEUE_SIZE;
    session->virtq_recv.avail = (struct virtq_avail*)GET_A_PAGE;
    session->virtq_recv.desc = (struct virtq_desc*)GET_A_PAGE;
    session->virtq_recv.used = (struct virtq_used*)GET_A_PAGE;

    session->net_hdrs = (struct virtio_net_hdr*)GET_A_PAGE;
    assert_int_ex(MEM_REQUEST_MIN_REQUEST, >, sizeof(struct virtio_net_hdr) * ((QUEUE_SIZE * 2)));
    session->net_hdrs_paddr = mem_paddr_for_vaddr((size_t)session->net_hdrs);

    u32 features = (1 << VIRTIO_NET_F_MRG_RXBUF);
    int result = virtio_device_init(session->mmio, net, 1, VIRTIO_QEMU_VENDOR, features);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add(session->mmio, 0, &session->virtq_recv);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_queue_add(session->mmio, 1, &session->virtq_send);
    assert_int_ex(-result, ==, 0);
    result = virtio_device_device_ready(session->mmio);
    assert_int_ex(-result, ==, 0);

    virtio_q_init_free(&session->virtq_recv, &session->free_head_recv, 0);
    virtio_q_init_free(&session->virtq_send, &session->free_head_send, 0);
    session->recvs_free = QUEUE_SIZE;

    session->config = *(struct virtio_net_config*)session->mmio->config;

    syscall_interrupt_register(session->irq, act_self_ctrl, -1, 0, nif);

    return 0;
}

err_t myif_link_output(struct netif *netif, struct pbuf *p);


static void free_send(net_session* session) {
    struct virtq* sendq = &session->virtq_send;

    // First free any pbufs/descs on the out path that have finished
    while(sendq->last_used_idx != sendq->used->idx) {
        // Free pbufs that have been used
        le16 used_idx = (le16)(sendq->last_used_idx & (sendq->num-1));
        struct virtq_used_elem used = sendq->used->ring[used_idx];

        struct pbuf* pb = session->pbuf_send_map[used.id];
        struct virtio_net_hdr* net_hdr = &session->net_hdrs[SEND_HDR_START + (used.id)];

        pbuf_free(pb);

        virtio_q_free_chain(sendq, &session->free_head_send, (le16)used.id);

        sendq->last_used_idx++;
    }
}

static void alloc_recv(net_session* session) {

    // enough for a least 1 more recieve chain
    while (session->recvs_free >= 3) {
        struct pbuf* pb = pbuf_alloc(PBUF_RAW, TCP_MSS + PBUF_TRANSPORT, PBUF_RAM);
        le16 head = virtio_q_alloc(&session->virtq_recv, &session->free_head_recv);

        assert_int_ex(head, !=, session->virtq_recv.num);

        session->pbuf_recv_map[head] = pb;

        le16 tail = head;


        struct virtq_desc* desc = session->virtq_recv.desc+head;

        desc->addr = NET_HDR_P(head);
        desc->len = sizeof(struct virtio_net_hdr);
        desc->flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;

        int ret = virtio_q_chain_add_virtual(&session->virtq_recv, &session->free_head_recv, &tail,
                                   pb->payload + ETH_PAD_SIZE, pb->len - ETH_PAD_SIZE, VIRTQ_DESC_F_WRITE);

        assert_int_ex(ret, >, 0);

        virtio_q_add_descs(&session->virtq_recv, (le16)head);

        session->recvs_free -= (ret + 1);
    }

    virtio_device_notify(session->mmio, 0);
}

int init_net(net_session* session, struct netif* nif) {
    lwip_init();


    nif = netif_add(nif, &session->my_ip, &session->netmask, &session->gw_addr,
                    (void*)session, &netsession_init, &ethernet_input);
    if(nif == NULL) return -1;

    memcpy(nif->hwaddr, session->mac, 6);
    nif->flags |= NETIF_FLAG_ETHARP;

    netif_set_default(nif);

    dhcp_start(nif);

    nif->linkoutput = &myif_link_output;
    nif->output = &etharp_output;

    netif_set_default(nif);

    alloc_recv(session);

    sys_restart_timeouts();
    netif_set_link_up(nif);
    netif_set_up(nif);

    printf("Only now enabled\n");
    syscall_interrupt_enable(session->irq, act_self_ctrl);
    virtio_device_ack_used(session->mmio);

    return 0;
}

err_t myif_link_output(struct netif *netif, struct pbuf *p) {


    net_session* session = netif->state;

    // Try reclaim as many send buffers as possible
    free_send(session);

    struct virtq* sendq = &session->virtq_send;

    le16 head = virtio_q_alloc(sendq, &session->free_head_send);

    assert(head != QUEUE_SIZE);

    le16 tail = head;

    // Put in a reference until this send has finished

    session->pbuf_send_map[head] = p;

    struct virtq_desc* desc = sendq->desc + head;

    struct virtio_net_hdr* net_hdr = &session->net_hdrs[SEND_HDR_START + (head)];

    bzero(net_hdr, sizeof(struct virtio_net_hdr));

    desc->addr = NET_HDR_P(SEND_HDR_START + (head));
    desc->len = sizeof(struct virtio_net_hdr);
    desc->flags = VIRTQ_DESC_F_NEXT;

    // TODO crossing a page problem
    // TODO probably have to bump reference count on pbuf until this write finishes.

    int first_pbuf = 1;
    do {
        pbuf_ref(p); // maybe every one?
        capability payload = (capability)(((char*)p->payload));
        le32 size = (le32)(p->len);
        if(first_pbuf) {
            payload += ETH_PAD_SIZE;
            size -=ETH_PAD_SIZE;
            first_pbuf = 0;
        }
        int res = virtio_q_chain_add_virtual(sendq, &session->free_head_send, &tail, payload, size, VIRTQ_DESC_F_NEXT);

        if(res < 0) {
            // We are out of buffers =(
            virtio_q_free(sendq, &session->free_head_send, head, tail);
            return ERR_MEM;
        }
    } while(p->len != p->tot_len && (p = p->next));

    sendq->desc[tail].flags = 0;

    virtio_q_add_descs(sendq, (le16)head);

    // Notify device there are packets to send
    virtio_device_notify(session->mmio, 1);

    return ERR_OK;
}

void interrupt(struct netif* nif) {

    net_session* session = nif->state;

    free_send(session);

    // Then process incoming packets and pass them up to lwip
    struct virtq* recvq = &session->virtq_recv;
    int any_in = 0;
    while(recvq->last_used_idx != recvq->used->idx) {
        any_in = 1;

        size_t used_idx = recvq->last_used_idx & (recvq->num-1);

        struct virtq_used_elem used = recvq->used->ring[used_idx];

        struct pbuf* pb = session->pbuf_recv_map[used.id];
        struct virtio_net_hdr* net_hdr = &session->net_hdrs[used.id];

        assert_int_ex(net_hdr->num_buffers, ==, 1); // Otherwise we will have to gather and I CBA

        // NOTE: input will free its pbuf!
        nif->input(pb, nif);

        session->recvs_free += virtio_q_free_chain(recvq, &session->free_head_recv, used.id);
        recvq->last_used_idx++;
    }

    if(any_in) alloc_recv(session);
    virtio_device_ack_used(session->mmio);
    syscall_interrupt_enable(session->irq, act_self_ctrl);
}

uni_dir_socket_requester* user_tcp_new(uni_dir_socket_fulfiller* tcp_output_pushee,
                                       uni_dir_socket_requester* tcp_input_pusher,
                                       ip_addr_t ip_addr, u16_t port) {
    tcp_session* session = alloc_tcp_session(tcp_output_pushee, tcp_input_pusher);

    session->byte_mark = session->byte_missing = session->byte_highest_seen = 0;
    session->events = POLL_IN;
    session->connected = 0;
    session->con = NULL;
    session->tcp_pcb = tcp_new();
    tcp_arg(session->tcp_pcb, session);
    tcp_bind(session->tcp_pcb, &ip_addr, port);

    return socket_internal_make_read_only(&session->tcp_output_pusher.r);
}

enum TCP_OOBS {
    REQUEST_TCP_CONNECT = (1 << 16) | (0x10),
    REQUEST_TCP_LISTEN  = (1 << 16) | (0x11),
};

err_t tcp_sent_callback (void *arg, struct tcp_pcb *tpcb,
                             u16_t len) {
    tcp_session* tcp = (tcp_session*)arg;

    // TODO check whether this is len TOTAL bytes or len more bytes (assuming here that its a delta)

    // Progress len bytes
    ssize_t res = socket_internal_fulfill_progress_bytes(&tcp->tcp_input_pushee, len,
                                            F_PROGRESS | F_DONT_WAIT,
                                           NULL, NULL, 0, &ful_oob_func_skip_oob);
    assert_int_ex(res, ==, len);

    return ERR_OK;
}

ssize_t tcp_ful_func(capability arg, char* buf, uint64_t offset, uint64_t length) {
    tcp_session* tcp = (tcp_session*)arg;

    uint64_t start = tcp->byte_mark; // need to attach (start,length) to the pbuf we create
    tcp->byte_mark+=length;
    // Make a pbuf to send, linked to a byte range
}

err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    tcp_session* tcp = (tcp_session*)arg;
    tcp->con->result = err;
    assert_int_ex(err, ==, ERR_OK); // TODO handle
    tcp->events = POLL_IN;
    tcp->connected = 1;
    tcp->con = NULL;
    tcp_sent(tcp->tcp_pcb, tcp_sent_callback);
    // Ack connection
    socket_internal_fulfill_progress_bytes(&tcp->tcp_input_pushee, SOCK_INF,
                                           F_CHECK | F_PROGRESS | F_DONT_WAIT,
                                           &ful_func_cancel_non_oob, NULL, 0, &ful_oob_func_skip_oob);
    socket_internal_fulfiller_reset_check(&tcp->tcp_input_pushee);
    return err;
}

ssize_t tcp_ful_oob_func(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    tcp_session* tcp = (tcp_session*)arg;

    enum TCP_OOBS otype = (enum TCP_OOBS)request->type;
    struct tcp_con_args* args = (struct tcp_con_args*)request->request.oob; // ONLY VALID IF otype field correct
    err_t er;
    switch(otype) {
        case REQUEST_TCP_CONNECT:
            er = tcp_connect(tcp->tcp_pcb, &args->addr, args->port, &tcp_connected_callback);
            tcp->events = POLL_NONE;
            tcp->con = args;
            assert(er == ERR_OK);
            break;
        case REQUEST_TCP_LISTEN:
            tcp->tcp_pcb = tcp_listen(tcp->tcp_pcb);
            tcp->events = POLL_NONE;
            tcp->con = args;
            assert(tcp->tcp_pcb != NULL);
            tcp_accept(tcp->tcp_pcb, &tcp_connected_callback);
            break;
        default:
            return E_OOB; // Can't handle anything else

    }
    return length;
}


void handle_fulfill(tcp_session* tcp) {
    if(tcp->connected) {
        // We expect only data
    } else{
        // We expect an oob
        ssize_t bytes_translated = socket_internal_fulfill_progress_bytes(&tcp->tcp_input_pushee, SOCK_INF,
                                               F_CHECK | F_START_FROM_LAST_MARK | F_SET_MARK | F_DONT_WAIT,
                                               &tcp_ful_func, tcp, 0, &ful_oob_func_skip_oob);
        assert_int_ex(-bytes_translated, <=, 0);
        // Note we have pushed another bytes_translated bytes
    }
    // Convert the push writes into pbufs them put them in the tcp stack
    // Also handle OOBS: tcp_connect, tcp_listen


}

int main(register_t arg, capability carg) {
    // Init session
    printf("LWIP Hello World!\n");

    net_session session;
    session.irq = (uint8_t)arg;
    session.mmio = (virtio_mmio_map*)carg;

    inet_aton(CHERIOS_NET_MASK, &session.netmask.addr);
    inet_aton(CHERIOS_IP, & session.my_ip.addr);
    inet_aton(CHERIOS_GATEWAY, &session.gw_addr.addr);
    static const uint8_t mac[6] = CHERIOS_MAC;
    memcpy(session.mac, mac, 6);

    struct netif nif;

    // Init LWIP (calls the rest of init session)
    int res = init_net(&session, &nif);

    assert_int_ex(res, ==, 0);

    while(try_get_fs() == NULL) {
        sleep(0);
    }

    httpd_init();

    int sock_sleep = 0;
    int sock_event = 0;

    uint8_t sol = 0;
    // Main loop
    while(1) {
        if(sol++ % 256 == 0) printf("Sign of life\n");
        sock_event = 0;

        sys_check_timeouts();

        // Wait for a message or socket

        // Respond to messages (may include interrupt getting called or a new socket being created)
        if(!msg_queue_empty()) {
            msg_entry(1);
        }

        // If our interrupt was set after incoming packets we miss them?
        // Someting goes horribly wrong here.

        HW_SYNC;

        while((session.virtq_send.last_used_idx != session.virtq_send.used->idx)
                || ((session.virtq_recv.last_used_idx != session.virtq_recv.used->idx))) {
            if(!msg_queue_empty()) {
                msg_entry(1);
            } else {
                interrupt(session.nif); // Fake an interrupt
            }
        }

        restart_poll:
        // respond to sockets
        FOR_EACH_TCP(tcp_session) {
            assert(0 && "WIP");
            enum poll_events revents = socket_internal_fulfill_poll(&tcp_session->tcp_input_pushee, tcp_session->events, sock_sleep, 1);
            if(revents && sock_sleep) {
                sock_sleep = 0;
                goto restart_poll;
            }
            if(revents & POLL_IN) {
                sock_event = 1;
                handle_fulfill(tcp_session);
            } else if(revents & (POLL_ER | POLL_HUP)) {
                // TODO
                assert(0);
            }
        }

        // FIXME this needs timeout so we can call sys_check_timeouts more promptly
        if(sock_sleep) {
            syscall_cond_wait(1, 80000000);
            //wait();
        } else if (!sock_event) sock_sleep = 1;


    }
}

void (*msg_methods[]) = {user_tcp_new};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, interrupt};
size_t ctrl_methods_nb = countof(ctrl_methods);