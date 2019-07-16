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


#include "sockets.h"
#include "unistd.h"
#include "cheric.h"
#include "stdlib.h"
#include "assert.h"
#include "mman.h"
#include "misc.h"
#include "syscalls.h"
#include "namespace.h"
#include "net.h"
#include "lwip/dns.h"
#include "lwip_driver.h"
#include "thread.h"
#include "deduplicate.h"
#include "lwip/inet_chksum.h"

enum session_close_state {
    SCS_NONE = 0,
    SCS_FULFILL_CLOSED =        0x1,  // We closed our fulfiller
    SCS_REQUEST_CLOSED =        0x2,  // We closed our requester
    SCS_USER_REQUEST_CLOSED =   0x4,  // User closed their request channel
    SCS_USER_FULFILL_CLOSED =   0x8,  // User closed their fulfill channel
    SCS_USER_CLOSING_REQUESTER =0x10, // User sent a close request (but have not yet closed their requester)
    SCS_REMOTE_CLOSE =          0x20, // remote end closed. might still have data to hand up to application
    SCS_PCB_LAYER_CLOSED =      0x40, // We closed the LWIP tcp layer
    SCS_NEED_FINAL_CLOSE =      0x80,
};

typedef struct tcp_session {
    struct tcp_pcb* tcp_pcb;
    struct tcp_session* next, *prev;

    act_kt callback;
    capability callback_arg;
    register_t callback_port;

    fulfiller_t tcp_input_pushee;
    requester_t tcp_output_pusher;

    enum poll_events events;

    struct pbuf * ack_head, *ack_tail;
    uint64_t recv;
    uint64_t application_recv_ack;
    uint64_t ack_handled;
    uint64_t free_leftover;

    enum session_close_state close_state;

    int session_id;
} tcp_session;

typedef struct tcp_listen_session {
    struct tcp_pcb* tcp_pcb;
    act_kt callback;
    capability callback_arg;
    register_t callback_port;
}tcp_listen_session;

sealing_cap sealer;
sealing_cap ether_sealer; // FIXME: Eventually owned by the ethernet driver. This is for proof of concept

static void user_tcp_close_send(tcp_session* tcp);
static void user_tcp_close_recv(tcp_session* tcp);
static void user_tcp_close(tcp_session* tcp);

tcp_session* tcp_head = NULL;
size_t n_listens = 0;
#define FOR_EACH_TCP(T) for(tcp_session* T = tcp_head; T != NULL; T = T->next)

static tcp_session* alloc_tcp_session(void) {

    // Allocate and add to chain
    tcp_session* new = (tcp_session*)malloc(sizeof(tcp_session));
    new->next = tcp_head;
    new->prev = NULL;
    if(tcp_head)tcp_head->prev = new;
    tcp_head = new;

    // Init
    new->tcp_input_pushee = socket_malloc_fulfiller(SOCK_TYPE_PUSH);
    new->tcp_output_pusher = socket_malloc_requester_32(SOCK_TYPE_PUSH, NULL);

    static int id = 0;
    new->session_id = id++;
    return new;
}

void free_tcp_session(tcp_session* tcp_session) {

    if(tcp_session->prev) tcp_session->prev->next = tcp_session->next;
    else tcp_head = tcp_session->next; // If we have no previous we are the head
    if(tcp_session->next) tcp_session->next->prev = tcp_session->prev;

    free(tcp_session->tcp_input_pushee);
    free(tcp_session->tcp_output_pusher);

    free(tcp_session);

    return;
}

err_t netsession_init(struct netif* nif) {
    assert(nif != NULL);

    // First init

    net_session* session = nif->state;
    session->nif = nif;

    session->dma_arena = new_arena(1);

    int res = lwip_driver_init(session);

    if(res < 0) {
        printf("LWIP: Got error: %d trying to init device\n", res);
        return ERR_IF;
    }

    return 0;
}

custom_for_tcp* custom_free_head;

static void free_malloc_pbuf(struct pbuf* pbuf) {
    custom_for_tcp* custom = (custom_for_tcp*)pbuf;
    if(custom->reuse-- == 0) {
        free((capability)pbuf);
    } else {
        custom->as_free.next_free = custom_free_head;
        custom_free_head = custom;
    }
}

struct custom_for_tcp* alloc_custom(net_session* session) {
    custom_for_tcp* c = custom_free_head;
    if(c) {
        custom_free_head = c->as_free.next_free;
    } else {
        size_t offset;
        c = (custom_for_tcp*)malloc_arena_dma(sizeof(custom_for_tcp), session->dma_arena, &offset);
        c->reuse = DEFAULT_USES;
        c->offset = offset;
    }

    c->as_pbuf.custom.custom_free_function = &free_malloc_pbuf;

    char* buffer = c->as_pbuf.buf;

#ifdef FORCE_PAYLOAD_CACHE_ALIGN
    size_t align = (CUSTOM_ALIGN - ((size_t)buffer)) & (CUSTOM_ALIGN - 1);
    buffer += align;
    buffer = cheri_setbounds(buffer, CUSTOM_BUF_PAYLOAD_SIZE);
#endif

    pbuf_alloced_custom(PBUF_RAW, CUSTOM_BUF_PAYLOAD_SIZE, PBUF_RAM, &c->as_pbuf.custom, buffer, CUSTOM_BUF_PAYLOAD_SIZE);

    return c;
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

    nif->linkoutput = &lwip_driver_output;
    nif->output = &etharp_output;

    netif_set_default(nif);

    sys_restart_timeouts();
    netif_set_link_up(nif);
    netif_set_up(nif);

    lwip_driver_init_postup(session);

    return 0;
}

static void finish_closing_user_requester(tcp_session* tcp) {
    if(tcp->tcp_pcb->snd_buf == TCP_SND_BUF) {
        user_tcp_close_send(tcp);
    }
}
// Application (ack) -> TCP
err_t tcp_sent_callback (void *arg, __unused struct tcp_pcb *tpcb,
                             u16_t len) {
    if(arg == NULL) return ERR_OK;

    tcp_session* tcp = (tcp_session*)arg;

    // This is how many bytes have been been sent this. NOT total.

    // Progress len bytes
    ssize_t res = socket_fulfill_progress_bytes_unauthorised(tcp->tcp_input_pushee, len,
                                            F_PROGRESS | F_DONT_WAIT | F_SKIP_OOB,
                                           NULL, NULL, 0, NULL, NULL,
                                           NULL, NULL);
    assert_int_ex(res, ==, len);

    if(tcp->close_state & SCS_USER_CLOSING_REQUESTER) {
        finish_closing_user_requester(tcp);
    }

    return ERR_OK;
}

// If there are no errors and the callback function returns ERR_OK, then it is responsible for freeing the pbuf.
// Otherwise, it must not free the pbuf so that lwIP core code can store it.

// TCP -> Application
err_t tcp_recv_callback(void * arg, __unused struct tcp_pcb * tpcb,
                        struct pbuf * p, err_t err) {
    if(arg == NULL) return err;

    tcp_session* tcp = (tcp_session*)arg;

    if(p == NULL) { // The other end has closed. So they are no longer receiving. Thus we close our fulfiller.
        user_tcp_close_send(tcp);
        tcp->close_state |= SCS_REMOTE_CLOSE;
        return ERR_OK;
    }

    struct pbuf* pp = p;
    uint16_t count = 1;
    while(pp->len != pp->tot_len) {
        pp = pp->next;
        count++;
    }

    int res = socket_requester_space_wait(tcp->tcp_output_pusher, count, 1, 0);

    assert_int_ex(-res, == , -0);

    // If we set P to be the head, it has only one reference (the reference that will free it).
    if(tcp->ack_head == NULL) tcp->ack_head = p;
    else {
        //  otherwise it has 2, the tail element from the ack chain, and also the reference for when we manually call free
        tcp->ack_tail->next = p;
        pbuf_ref(p);
    }

    pp = p;
    do {
        if(p != pp) {
            // We add an extra ref to everything in the chain, as we will free them individually
            pbuf_ref(pp);
        }
        if((pp->flags & PBUF_FLAG_IS_CUSTOM)) {
            // These must not be re-used if they go to user space.
            ((struct custom_for_tcp*)pp)->reuse = 0;
        }
        socket_request_ind(tcp->tcp_output_pusher, (char*)pp->payload, pp->len, pp->len);
        tcp->recv += pp->len;
    } while(pp->len != pp->tot_len && (pp = pp->next));

    assert(pp != NULL);

    tcp->ack_tail = pp;

    return ERR_OK;
}

// TCP ->(ack) Application
static void tcp_application_ack_all(tcp_session* tcp) {
    if(tcp->recv != tcp->ack_handled){
        uint64_t to_ack = (tcp->recv - tcp->ack_handled);
        tcp_recved(tcp->tcp_pcb, (u16_t)to_ack);
        tcp->ack_handled = tcp->recv;
    }
    struct pbuf* p = tcp->ack_head;
    struct pbuf* pp;
    while(p) {
        pp = p->next;
        pbuf_free(p); // This frees a whole chain, but we have added in extra references
        p = pp;
    }
    tcp->ack_head = NULL;
}

// TCP ->(ack) Application
static void tcp_application_ack(tcp_session* tcp) {

    if(tcp->application_recv_ack != tcp->ack_handled) {
        assert(tcp->application_recv_ack  <= tcp->recv && tcp->application_recv_ack > tcp->ack_handled);

        uint64_t to_ack = (tcp->application_recv_ack - tcp->ack_handled);

        assert_int_ex(to_ack, <=, UINT16_MAX);
        tcp_recved(tcp->tcp_pcb, (u16_t)to_ack);

        tcp->ack_handled = tcp->application_recv_ack;

        to_ack += tcp->free_leftover;
        struct pbuf* p = tcp->ack_head;
        while(to_ack && p->len <= to_ack) {
            to_ack -=p->len;
            tcp->ack_head = p->next;
            pbuf_free(p);
            p = tcp->ack_head;
        }
        tcp->free_leftover = to_ack;
    }
}

// Application -> TCP
ssize_t TRUSTED_CROSS_DOMAIN(tcp_ful_func)(capability arg, char* buf, uint64_t offset, uint64_t length);
ssize_t tcp_ful_func(capability arg, char* buf, __unused uint64_t offset, uint64_t length) {
    tcp_session* tcp = (tcp_session*)arg;

    // Make a pbuf to send, linked to a byte range

    uint16_t to_send = (length > (uint64_t)tcp->tcp_pcb->snd_buf) ? tcp->tcp_pcb->snd_buf : (uint16_t)length;

    assert(cheri_getsealed(buf));

    // TODO TCP_WRITE_FLAG_MORE may be useful here if we know that there is more coming
    err_t er = tcp_write(tcp->tcp_pcb, buf, (uint16_t)to_send, 0);

    if(er == ERR_MEM) {
        return 0;
    }

    assert_int_ex(er, ==, ERR_OK);

    return to_send;
}

ssize_t TRUSTED_CROSS_DOMAIN(tcp_ful_oob_func)(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length);
ssize_t tcp_ful_oob_func(capability arg, request_t* request, __unused uint64_t offset, __unused uint64_t partial_bytes, uint64_t length) {
    tcp_session* tcp = (tcp_session*)arg;

    switch(request->type) {
        case REQUEST_CLOSE:
            tcp->close_state |= SCS_USER_CLOSING_REQUESTER;
            break;
        case REQUEST_FLUSH:
            tcp_output(tcp->tcp_pcb);
            break;
        default:
            // Empty
        {}
    }

    return length;
}

// Application -> TCP
void handle_fulfill(tcp_session* tcp) {

    assert(!(tcp->close_state & (SCS_FULFILL_CLOSED | SCS_USER_CLOSING_REQUESTER)));

    // We expect only data
    __unused ssize_t bytes_translated = socket_fulfill_progress_bytes_unauthorised(tcp->tcp_input_pushee, SOCK_INF,
                                                                      F_CHECK | F_START_FROM_LAST_MARK | F_SET_MARK | F_DONT_WAIT,
                                                                      &TRUSTED_CROSS_DOMAIN(tcp_ful_func), tcp, 0, &TRUSTED_CROSS_DOMAIN(tcp_ful_oob_func),
                                                                      NULL, TRUSTED_DATA, TRUSTED_DATA);

    err_t flush = tcp_output(tcp->tcp_pcb);

    if(tcp->close_state & SCS_USER_CLOSING_REQUESTER) {
        finish_closing_user_requester(tcp);
    }

    assert_int_ex(flush, == , ERR_OK);
}

static tcp_session* user_tcp_new(struct tcp_pcb* pcb) {

    assert(pcb != NULL);

    tcp_session* session = alloc_tcp_session();

    session->application_recv_ack = session->recv= session->ack_handled = 0;
    session->ack_head = session->ack_tail = NULL;
    socket_requester_set_drb_ptr(session->tcp_output_pusher, &session->application_recv_ack);
    session->events = POLL_NONE;
    session->tcp_pcb = pcb;


    tcp_arg(session->tcp_pcb, session);
    tcp_sent(session->tcp_pcb, tcp_sent_callback);
    tcp_recv(session->tcp_pcb, tcp_recv_callback);
    // er is set elsewhere

    return session;
}

static void send_connect_callback(tcp_session* tcp, err_t err) {
    capability sealed_tcp = (capability)tcp;
    message_send((register_t)err, tcp->tcp_pcb->remote_port, tcp->tcp_pcb->remote_ip.addr, 0, tcp->callback_arg,
                 sealed_tcp, (capability)socket_make_ref_for_fulfill(tcp->tcp_output_pusher), NULL,
                 tcp->callback, SEND, tcp->callback_port);
    socket_requester_connect(tcp->tcp_output_pusher);
}

static void tcp_er(void *arg, __unused err_t err) {
    tcp_session* tcp = (tcp_session*)arg;

    // This session may have already been freed
    if(tcp != NULL) {
        // We don't free here. Instead we mark the pcb as already closed, and handle on the next main loop
        user_tcp_close_recv(tcp);
        user_tcp_close_send(tcp);
    }

}

static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if(arg == NULL) return err;

    tcp_session* tcp = (tcp_session*)arg;

    assert(tcp->tcp_pcb == tpcb);

    assert_int_ex(err, ==, ERR_OK); // TODO handle

    tcp_err(tcp->tcp_pcb, tcp_er);

    send_connect_callback(tcp, err);

    return err;
}

static void user_tcp_connect_er(void *arg, err_t err) {
    tcp_session* tcp = (tcp_session*)arg;

    // Send an error to the user
    message_send((register_t)err, 0, 0, 0, tcp->callback_arg,
                 NULL, NULL, NULL,
                 tcp->callback, SEND, tcp->callback_port);
    // I don't -think- we need to free the TCP layer. LWIP does this for us.

    // Then free session

    free_tcp_session(tcp);
}

static err_t tcp_accept_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {

    if(arg == NULL) return err;

    assert_int_ex(err, ==, ERR_OK); // TODO handle

    tcp_listen_session* listen_session = (tcp_listen_session*)arg;

    assert(listen_session->tcp_pcb != tpcb);

    assert(tpcb != NULL);

    tcp_session* tcp = user_tcp_new(tpcb);

    tcp->callback = listen_session->callback;
    tcp->callback_arg = listen_session->callback_arg;
    tcp->callback_port = listen_session->callback_port;

    tcp_err(tcp->tcp_pcb, tcp_er);

    send_connect_callback(tcp, err);

    return err;
}

static int user_tcp_connect_sockets(capability sealed_session,
                                      requester_t tcp_input_pusher) {

    tcp_session* tcp = (tcp_session*)sealed_session; // TODO seal/unseal
    tcp->events = POLL_IN;
    int res = socket_fulfiller_connect(tcp->tcp_input_pushee, tcp_input_pusher);
    if(res < 0) return res;
    if(tcp->close_state & SCS_FULFILL_CLOSED) {
        // We got the connect AFTER we tried to close! We have to close again to signal the pusher that we closed.
        socket_close_fulfiller(tcp->tcp_input_pushee, 0, 1);
    }

    return 0;
}

static err_t user_tcp_connect(struct tcp_bind* bind, struct tcp_bind* server,
                             act_kt callback, capability callback_arg, register_t callback_port) {
    tcp_session* tcp = user_tcp_new(tcp_new());
    assert(bind != NULL);
    tcp_bind(tcp->tcp_pcb, &bind->addr, bind->port);
    tcp->callback = callback;
    tcp->callback_arg = callback_arg;
    tcp->callback_port = callback_port;
    tcp_err(tcp->tcp_pcb, &user_tcp_connect_er);
    err_t er = tcp_connect(tcp->tcp_pcb, &server->addr, server->port, tcp_connected_callback);
    assert_int_ex(-er, ==, -ERR_OK);
    return er;
}

static uintptr_t user_tcp_listen(struct tcp_bind* bind, uint8_t backlog,
                            act_kt callback, capability callback_arg, register_t callback_port) {
    tcp_listen_session* listen_session = (tcp_listen_session*)(malloc(sizeof(tcp_listen_session)));

    listen_session->callback = callback;
    listen_session->callback_arg = callback_arg;
    listen_session->callback_port = callback_port;

    listen_session->tcp_pcb = tcp_new();
    err_t er = tcp_bind(listen_session->tcp_pcb, &bind->addr, bind->port);

    assert_int_ex(-er, ==, -ERR_OK);

    listen_session->tcp_pcb = tcp_listen_with_backlog(listen_session->tcp_pcb, backlog);
    tcp_arg(listen_session->tcp_pcb, listen_session);
    tcp_accept(listen_session->tcp_pcb, tcp_accept_callback);

    if(er != ERR_OK) return (uintptr_t)er;

    n_listens++;

    return (uintptr_t)cheri_seal(listen_session, sealer);
}

static void stop_listening(capability sealed) {
    tcp_listen_session* listen_session = cheri_unseal(sealed, sealer);

    err_t er = tcp_close(listen_session->tcp_pcb);

    assert(er == ERR_OK);

    n_listens--;

    free(listen_session);

    return;
}

static void user_tcp_close_send(tcp_session* tcp) {

    ssize_t res;

    if(!(tcp->close_state & SCS_FULFILL_CLOSED)) {
        // We will have done this earlier otherwise
        res = socket_close_fulfiller(tcp->tcp_input_pushee, 0, 1);
        tcp->close_state |= SCS_FULFILL_CLOSED;
        assert_int_ex(res, ==, 0);
        if(!(tcp->close_state & SCS_PCB_LAYER_CLOSED)) tcp_shutdown(tcp->tcp_pcb,0,1);
    }

    if(tcp->close_state & SCS_REQUEST_CLOSED) {
        tcp->close_state |= SCS_NEED_FINAL_CLOSE;
    }
}

static void user_tcp_close_recv(tcp_session* tcp) {

    ssize_t res;

    // Then free any pcbs / ack the entire window in case anything has not been consumed
    tcp_application_ack_all(tcp);

    if(!(tcp->close_state & SCS_REQUEST_CLOSED)) {
        res = socket_close_requester(tcp->tcp_output_pusher, 0, 1);
        tcp->close_state |= SCS_REQUEST_CLOSED;
        assert_int_ex(res, ==, 0);
        if(!(tcp->close_state & SCS_PCB_LAYER_CLOSED)) tcp_shutdown(tcp->tcp_pcb,1,0);
    }

    if(tcp->close_state & SCS_FULFILL_CLOSED) {
        tcp->close_state |= SCS_NEED_FINAL_CLOSE;
    }
}

static void user_tcp_close(tcp_session* tcp) {

    // Make sure both directions have been closed

    user_tcp_close_recv(tcp);
    user_tcp_close_send(tcp);

    // Free our session
    free_tcp_session(tcp);

    return;
}

static void dns_lookup_callback(__unused const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    sync_state_t ss;
    ss.sync_caller = callback_arg;
    msg_resume_return(NULL, ipaddr->addr, 0, ss);
}

static void user_gethostbyname(const char* name) {

    ip_addr_t ip;
    sync_state_t ss;

    msg_delay_return(&ss);

    err_t er = dns_gethostbyname(name, &ip, &dns_lookup_callback, (void*)ss.sync_caller);

    if(er == ERR_OK) {
        msg_resume_return(NULL, ip.addr, 0, ss);

    } else if(er != ERR_INPROGRESS) {
        msg_resume_return(NULL, (u32_t)-1, 0, ss);
    }
}

extern char checksum_foundation_data;
extern char checksum_foundation_data_end;
extern void checksum_found_enter_offset;

void setup_checksum_found(sealing_cap sc) {
    capability if_table[7];
    capability data_arg;

    char* start = &checksum_foundation_data;
    char* stop = &checksum_foundation_data_end;
    size_t entry_off = (size_t)&checksum_found_enter_offset;

    size_t length = stop - start;


    res_t res = cap_malloc(FOUNDATION_META_SIZE(1,length) + length);

    entry_t entry = foundation_create(res, length, start, entry_off, 1, 0);
    assert(entry != NULL);

    foundation_enter(if_table, sc, &data_arg, NULL, NULL, entry, NULL);

    SET_SYM(checksum_found_data, data_arg);
    SET_FUNC(checksum_extern_make_new, if_table[0]);
    SET_FUNC(checksum_extern_free, if_table[1]);
    SET_FUNC(checksum_extern_set, if_table[2]);
    SET_FUNC(checksum_extern_add_int, if_table[3]);
    SET_FUNC(checksum_extern_xor_int, if_table[4]);
    SET_FUNC(checksum_extern_swap_bytes, if_table[5]);
    SET_FUNC(checksum_extern_add_buffer, if_table[6]);

}

int main(__unused register_t arg, __unused capability carg) {
    // Init session
    printf("LWIP Hello World!\n");

    msg_allow_more_sends();

    ether_sealer = get_type_owned_by_process();

    setup_checksum_found(ether_sealer);

    net_session session;

    bzero(&session, sizeof(net_session));

    inet_aton(CHERIOS_NET_MASK, &session.netmask.addr);
    inet_aton(CHERIOS_IP, & session.my_ip.addr);
    inet_aton(CHERIOS_GATEWAY, &session.gw_addr.addr);
    static const uint8_t mac[6] = CHERIOS_MAC;
    memcpy(session.mac, mac, 6);

    struct netif nif;
    bzero(&nif, sizeof(struct netif));

    // Init LWIP (calls the rest of init session)
    int res = init_net(&session, &nif);

    assert_int_ex(res, ==, 0);

    while(try_get_fs() == NULL) {
        sleep(MS_TO_CLOCK(100));
    }

    httpd_init();

    // Advertise self for tcp/socket layer
    namespace_register(namespace_num_tcp, act_self_ref);

    // Get a type to seal with
    sealer = get_type_owned_by_process();

    printf("LWIP Should now be responsive\n");



    int ints = 0;
    int fake_ints = 0;

    register_t time = syscall_now();
    // Main loop
    POLL_LOOP_START(sock_sleep, sock_event, 1)

        register_t now = syscall_now();
        if((now - time) >= MS_TO_CLOCK(10 * 1000)) { // Give a status report every 10 seconds
            time = now;
            size_t n = 0;
            FOR_EACH_TCP(T) {
                n++;
            }
            printf("Sign of life now = %lx. (%d)(%d) Listeners = %ld. Open TCPS = %lx\n",
                   now, ints, fake_ints, n_listens, n);
            stats_display();
        }

        sys_check_timeouts();

        netif_poll_all();
        // Wait for a message or socket

        // If our interrupt was set after incoming packets we miss them?
        // Someting goes horribly wrong here.

        HW_SYNC;

        while(lwip_driver_poll(&session)) {
            if(!msg_queue_empty()) {
                ints++;
                msg_entry(0, 0);
            } else {
                fake_ints++;
                lwip_driver_handle_interrupt(&session, 0, session.irq); // Fake an interrupt
            }
        }

        restart_poll:

        if(sock_sleep) syscall_cond_cancel();

        // respond to sockets
        FOR_EACH_TCP(tcp_session) {
            // The user stops reading
            if(!(tcp_session->close_state & SCS_USER_FULFILL_CLOSED) &&
                    socket_requester_is_fulfill_closed(tcp_session->tcp_output_pusher)) {
                tcp_session->close_state |= SCS_USER_FULFILL_CLOSED;
                user_tcp_close_recv(tcp_session);
            }

            // Remote has closed and we have finished having
            if((tcp_session->close_state & (SCS_REMOTE_CLOSE)) &&
                    !(tcp_session->close_state & (SCS_USER_FULFILL_CLOSED | SCS_REQUEST_CLOSED)) &&
                        (socket_requester_wait_all_finish(tcp_session->tcp_output_pusher, 1) == 0)) {
                user_tcp_close_recv(tcp_session);
            }

            if(tcp_session->close_state & SCS_NEED_FINAL_CLOSE) {
                user_tcp_close(tcp_session);
                // Don't trust the for each after modifying the set
                goto restart_poll;
            }

            if(!(tcp_session->close_state & (SCS_REQUEST_CLOSED))) {
                tcp_application_ack(tcp_session);
            }

            if(!(tcp_session->close_state & (SCS_FULFILL_CLOSED | SCS_USER_REQUEST_CLOSED | SCS_PCB_LAYER_CLOSED))) {
                if(tcp_session->tcp_pcb->snd_buf) { // dont even bother if the send window is already full
                    POLL_ITEM_F(revents, sock_sleep, sock_event, tcp_session->tcp_input_pushee, tcp_session->events, 1);
                    if(revents & POLL_IN) {
                        handle_fulfill(tcp_session);
                    } else if(revents & (POLL_ER | POLL_HUP)) {
                        // The user shouldn't close their requester unless there has been an error, so close everything
                        printf("Poll err %d %d:%d\n", revents,
                               tcp_session->tcp_pcb->remote_port, tcp_session->tcp_pcb->local_port);
                        tcp_session->close_state |= SCS_USER_REQUEST_CLOSED;
                        user_tcp_close(tcp_session);
                        // Don't trust the for each after modifying the set
                        goto restart_poll;
                    } else if(revents){
                        printf("Got an unexpected event: %d\n", revents);
                        sleep(1000);
                        assert(0);
                    }
                }
            }

        }

    POLL_LOOP_END(sock_sleep, sock_event, 1, MS_TO_CLOCK(250)); // Roughly enough for most TCP things
}

static sealing_cap user_get_ether_sealer(void) {
    return cheri_andperm(ether_sealer, CHERI_PERM_SEAL);
}

void (*msg_methods[]) = {user_tcp_connect, user_tcp_listen, user_tcp_connect_sockets, user_gethostbyname, stop_listening, user_get_ether_sealer};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, lwip_driver_handle_interrupt};
size_t ctrl_methods_nb = countof(ctrl_methods);