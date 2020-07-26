#include "cheric.h"
#include "namespace.h"
#include "syscalls.h"
#include "net.h"
#include "sockets.h"
#include "assert.h"
#include "stdio.h"
#include "cheristd.h"

#define TRACE_HCI 0

static NET_SOCK netsock = NULL;


/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "os/os_mempool.h"
#include "nimble/ble.h"
#include "nimble/ble_hci_trans.h"
#include "nimble/hci_common.h"

/* HCI packet types */
#define HCI_PKT_CMD     0x01
#define HCI_PKT_ACL     0x02
#define HCI_PKT_EVT     0x04
#define HCI_PKT_GTL     0x05

/* Buffers for HCI commands data */
static uint8_t trans_buf_cmd[BLE_HCI_TRANS_CMD_SZ] __attribute__((aligned(16)));
static uint8_t trans_buf_cmd_allocd;

/* Buffers for HCI events data */
static uint8_t trans_buf_evt_hi_pool_buf[ OS_MEMPOOL_BYTES(
                                            MYNEWT_VAL(BLE_HCI_EVT_HI_BUF_COUNT),
                                            MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE)) ];
static struct os_mempool trans_buf_evt_hi_pool;
static uint8_t trans_buf_evt_lo_pool_buf[ OS_MEMPOOL_BYTES(
                                            MYNEWT_VAL(BLE_HCI_EVT_LO_BUF_COUNT),
                                            MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE)) ];
static struct os_mempool trans_buf_evt_lo_pool;

/* Buffers for HCI ACL data */
#define ACL_POOL_BLOCK_SIZE OS_ALIGN(MYNEWT_VAL(BLE_ACL_BUF_SIZE) + \
                                            BLE_MBUF_MEMBLOCK_OVERHEAD + \
                                            BLE_HCI_DATA_HDR_SZ, OS_ALIGNMENT)
static uint8_t trans_buf_acl_pool_buf[ OS_MEMPOOL_BYTES(
                                            MYNEWT_VAL(BLE_ACL_BUF_COUNT),
                                            ACL_POOL_BLOCK_SIZE) ];
static struct os_mempool trans_buf_acl_pool;
static struct os_mbuf_pool trans_buf_acl_mbuf_pool;

/* Host interface */
static ble_hci_trans_rx_cmd_fn *trans_rx_cmd_cb;
static void *trans_rx_cmd_arg;
static ble_hci_trans_rx_acl_fn *trans_rx_acl_cb;
static void *trans_rx_acl_arg;

spinlock_t socket_spinlock;

/* Called by NimBLE host to reset HCI transport state (i.e. on host reset) */
int
ble_hci_trans_reset(void)
{
    static int fixme = 0;
    if(fixme++ > 0) return 0;
    printf("[nimble] %s IN\n"KRST, __func__);

    spinlock_acquire(&socket_spinlock);

    if (netsock != NULL) {
        close_file((FILE_t)netsock);
    }

    // Connect to server
    struct tcp_bind bind;
    struct tcp_bind server;

    bind.port = 0;
    bind.addr.addr = IP_ADDR_ANY->addr;

    inet_aton("192.168.18.55", &server.addr);
    server.port = 6666;

    while(net_try_get_ref() == NULL) {
        sleep(100);
    }
    // We need to loop. The server may not have been created yet!
    NET_SOCK local_netsock;
    do {
        printf("Trying to connect to HCI socket\n");
        sleep(100*1000*1000);
        netsock_connect_tcp(&bind, &server, NULL);
        local_netsock = netsock_accept(MSG_NONE);
    } while (local_netsock == NULL);

    needs_drb((FILE_t)local_netsock);


    netsock = local_netsock;

    printf("[nimble] %s OK!\n", __func__);
    spinlock_release(&socket_spinlock);
    return 0;
}

/* Called by NimBLE host to setup callbacks from HCI transport */
void
ble_hci_trans_cfg_hs(ble_hci_trans_rx_cmd_fn *cmd_cb, void *cmd_arg,
                     ble_hci_trans_rx_acl_fn *acl_cb, void *acl_arg)
{
    trans_rx_cmd_cb = cmd_cb;
    trans_rx_cmd_arg = cmd_arg;
    trans_rx_acl_cb = acl_cb;
    trans_rx_acl_arg = acl_arg;
}

/*
 * Called by NimBLE host to allocate buffer for HCI Command packet.
 * Called by HCI transport to allocate buffer for HCI Event packet.
 */
uint8_t *
ble_hci_trans_buf_alloc(int type)
{
    uint8_t *buf;

    switch (type) {
    case BLE_HCI_TRANS_BUF_CMD:
        assert(!trans_buf_cmd_allocd);
        trans_buf_cmd_allocd = 1;
        buf = trans_buf_cmd;
        break;
    case BLE_HCI_TRANS_BUF_EVT_HI:
        buf = os_memblock_get(&trans_buf_evt_hi_pool);
        if (buf) {
            break;
        }
        /* no break */
    case BLE_HCI_TRANS_BUF_EVT_LO:
        buf = os_memblock_get(&trans_buf_evt_lo_pool);
        break;
    default:
        assert(0);
        buf = NULL;
    }

    return buf;
}

/*
 * Called by NimBLE host to free buffer allocated for HCI Event packet.
 * Called by HCI transport to free buffer allocated for HCI Command packet.
 */
void
ble_hci_trans_buf_free(uint8_t *buf)
{
    int rc;

    if (buf == trans_buf_cmd) {
        assert(trans_buf_cmd_allocd);
        trans_buf_cmd_allocd = 0;
    } else if (os_memblock_from(&trans_buf_evt_hi_pool, buf)) {
        rc = os_memblock_put(&trans_buf_evt_hi_pool, buf);
        assert(rc == 0);
    } else {
        assert(os_memblock_from(&trans_buf_evt_lo_pool, buf));
        rc = os_memblock_put(&trans_buf_evt_lo_pool, buf);
        assert(rc == 0);
    }
}

static void send_on_hci_socket(void *buffer, size_t size)
{
    spinlock_acquire(&socket_spinlock);
    NET_SOCK local_netsock = netsock;
    spinlock_release(&socket_spinlock);
    uint8_t header[4] = {
        (size >>  0) & 0xFF,
        (size >>  8) & 0xFF,
        (size >> 16) & 0xFF,
        (size >> 24) & 0xFF,
    };
    ssize_t  res = socket_send(&local_netsock->sock, (char *) header, sizeof(header), MSG_NONE);
    assert_int_ex(res, ==, 4);
    ssize_t  res2 = socket_send(&local_netsock->sock, (char *) buffer, size, MSG_NONE);
    //printf(KRED"%s: res2:%zd\n", __func__, res2); flush_file(stdout);
    assert_int_ex(res2, ==, size);
}

/* Called by NimBLE host to send HCI Command packet over HCI transport */
int
ble_hci_trans_hs_cmd_tx(uint8_t *cmd)
{
    uint8_t *buf = cmd;

    /*
     * Buffer pointed by 'cmd' contains complete HCI Command packet as defined
     * by Core spec.
     */
    uint8_t payload_len = cmd[2];
#if TRACE_HCI > 0
    uint16_t opcode = ((uint16_t) cmd[1] << 8) + cmd[0];
    printf(KYLW"TX CMD Packet: opcode:%04x len:%u payload:", opcode, payload_len);
    for (size_t i=0; i<payload_len; i++) {
        printf("%02x ", cmd[3+i]);
    } printf("\n"KRST);
#endif

    uint16_t cmd_len = ((uint16_t) payload_len) + 3;

    uint8_t socket_buffer[1+3+255];
    socket_buffer[0] = HCI_PKT_CMD;
    memcpy(socket_buffer+1, cmd, cmd_len);

    size_t socket_buffer_fill = 1 + cmd_len;

    send_on_hci_socket(socket_buffer, socket_buffer_fill);

    ble_hci_trans_buf_free(buf);

    return 0;
}

/* Called by NimBLE host to send HCI ACL Data packet over HCI transport */
int
ble_hci_trans_hs_acl_tx(struct os_mbuf *om)
{
#if 0
    uint8_t *xsocket_buffer = calloc(1, 10+65535);
    if (xsocket_buffer == NULL) {
        printf(KRED"%s: socket_buffer is NULL\n", __func__); //TODO: handle properly
        goto xdone;
    }
xdone:
    free(xsocket_buffer);
    os_mbuf_free_chain(om);
    return 0;
#endif
    /*
     * mbuf pointed by 'om' contains complete HCI ACL Data packet as defined
     * by Core spec.
     */
    uint8_t *socket_buffer = calloc(1, 10+65535);
    if (socket_buffer == NULL) {
        printf(KRED"%s: socket_buffer is NULL\n", __func__); //TODO: handle properly
        goto done;
    }
    size_t socket_buffer_fill = 0;

    /* Prepare buffer */
    socket_buffer[socket_buffer_fill++] = HCI_PKT_ACL;
    for (struct os_mbuf *m = om; m; m = SLIST_NEXT(m, om_next)) {
        //todo: check socket_buffer capacity;
        memcpy(socket_buffer+socket_buffer_fill, m->om_data, m->om_len);
        socket_buffer_fill += m->om_len;
    }

#if TRACE_HCI > 0
    uint16_t handle = (((uint16_t)(socket_buffer[2]&0xFU)) << 8) + socket_buffer[1];
    uint8_t flags = socket_buffer[2] >> 4;
    size_t payload_len = (((size_t)socket_buffer[4]) << 8) + socket_buffer[3];
    printf(KYLW"TX ACL Packet: handle:0x%03x flags:0x%x len:%zu payload:", handle, flags, payload_len);
    for (size_t i=0; i<payload_len; i++) {
        printf("%02x ", socket_buffer[5+i]);
    } printf("\n"KRST); flush_file(stdout);
#endif

    send_on_hci_socket(socket_buffer, socket_buffer_fill);

done:
    free(socket_buffer);
    os_mbuf_free_chain(om);

    return 0;
}

/* Called by application to send HCI ACL Data packet to host */
static
int hci_transport_send_acl_to_host(uint8_t *buf, uint16_t size)
{
    struct os_mbuf *trans_mbuf;
    int rc;

    trans_mbuf = os_mbuf_get_pkthdr(&trans_buf_acl_mbuf_pool,
                                    sizeof(struct ble_mbuf_hdr));
    os_mbuf_append(trans_mbuf, buf, size);
    rc = trans_rx_acl_cb(trans_mbuf, trans_rx_acl_arg);

    return rc;
}

/* Called by application to send HCI Event packet to host */
__attribute__((noinline))
static int hci_transport_send_evt_to_host(uint8_t *buf, uint8_t size)
{
    uint8_t *trans_buf;
    int rc;

    /* Allocate LE Advertising Report Event from lo pool only */
    if ((buf[0] == BLE_HCI_EVCODE_LE_META) &&
        (buf[2] == BLE_HCI_LE_SUBEV_ADV_RPT)) {
        trans_buf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_LO);
        if (!trans_buf) {
            /* Skip advertising report if we're out of memory */
            return 0;
        }
    } else {
        trans_buf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_HI);
    }

    memcpy(trans_buf, buf, size);

    rc = trans_rx_cmd_cb(trans_buf, trans_rx_cmd_arg);
    if (rc != 0) {
        ble_hci_trans_buf_free(trans_buf);
    }

    return rc;
}

static int hci_rx_read_from_socket(uint8_t *buffer, size_t size)
{
    if (size == 0) {
        return 0;
    }
    spinlock_acquire(&socket_spinlock);
    NET_SOCK local_netsock = netsock;
    spinlock_release(&socket_spinlock);
    //printf(KCYN"%s go to recv\n"KRST, __func__); flush_file(stdout);
    ssize_t fill = socket_recv(&local_netsock->sock, (char *)buffer, size, MSG_NONE);
    //printf(KCYN"%s out of recv\n"KRST, __func__); flush_file(stdout);
    if (fill != size) {
        printf(KCYN"%s @socket_recv error :%zd\n"KRST, __func__, fill); flush_file(stdout);
        return -1;
    }
    return 0;
}

#if 0
static int hci_rx_handle_cmd_packet(uint8_t *socket_buffer)
{
    int ret = hci_rx_read_from_socket(socket_buffer, 3);
    if (ret != 0) {
        printf(KCYN"%s Failed to read HCI EVT packet header\n"KRST, __func__); flush_file(stdout);
        return -1;
    }

    size_t payload_len = socket_buffer[2];
    ret = hci_rx_read_from_socket(socket_buffer+3, payload_len);
    if (ret != 0) {
        printf(KCYN"%s Failed to read HCI EVT packet payload\n"KRST, __func__); flush_file(stdout);
        return -1;
    }

    uint16_t opcode = socket_buffer[0] + (((uint16_t)socket_buffer[1]) << 8);
#if TRACE_HCI > 0
    printf(KCYN"RX EVT Packet: opcode:0x%04x len:%zu payload:", opcode, payload_len);
    for (size_t i=0; i<payload_len; i++) {
        printf("%02x ", socket_buffer[1+3+i]);
    } printf("\n"KRST); flush_file(stdout);
#endif

    hci_transport_send_evt_to_host(socket_buffer, 3+payload_len);
    return 0;
}
#endif

__attribute__((noinline))
static int hci_rx_handle_evt_packet(uint8_t *socket_buffer)
{
    size_t header_len = 2;
    int ret = hci_rx_read_from_socket(socket_buffer, header_len);
    if (ret != 0) {
        printf(KCYN"%s Failed to read HCI EVT packet header\n"KRST, __func__); flush_file(stdout);
        return -1;
    }

    size_t payload_len = socket_buffer[1];
    ret = hci_rx_read_from_socket(socket_buffer+header_len, payload_len);
    if (ret != 0) {
        printf(KCYN"%s Failed to read HCI EVT packet payload\n"KRST, __func__); flush_file(stdout);
        return -1;
    }

#if TRACE_HCI > 0
    uint8_t event_code = socket_buffer[0];
    printf(KCYN"RX EVT Packet: event_code:0x%02x len:%zu payload:", event_code, payload_len);
    for (size_t i=0; i<payload_len; i++) {
        printf("%02x ", socket_buffer[header_len+i]);
    } printf("\n"KRST); flush_file(stdout);
#endif

    hci_transport_send_evt_to_host(socket_buffer, header_len+payload_len);
    return 0;
}

__attribute__((noinline))
static int hci_rx_handle_acl_packet(uint8_t *socket_buffer)
{
    size_t header_len = 4;
    int ret = hci_rx_read_from_socket(socket_buffer, header_len);
    if (ret != 0) {
        printf(KCYN"%s Failed to read HCI ACL packet header\n"KRST, __func__); flush_file(stdout);
        return -1;
    }

    size_t payload_len = (((size_t)socket_buffer[3]) << 8) + socket_buffer[2];

    ret = hci_rx_read_from_socket(socket_buffer+header_len, payload_len);
    if (ret != 0) {
        printf(KCYN"%s Failed to read HCI ACL packet payload\n"KRST, __func__); flush_file(stdout);
        return -1;
    }

#if TRACE_HCI > 0
    uint16_t handle = (((uint16_t)(socket_buffer[1]&0xFU)) << 8) + socket_buffer[0];
    uint8_t flags = socket_buffer[1] >> 4;
    printf(KCYN"RX ACL Packet: handle:0x%03x flags:0x%x len:%zu payload:", handle, flags, payload_len);
    for (size_t i=0; i<payload_len; i++) {
        printf("%02x ", socket_buffer[header_len+i]);
    } printf("\n"KRST); flush_file(stdout);
#endif /* TRACE_HCI > 0 */

    hci_transport_send_acl_to_host(socket_buffer, header_len+payload_len);
    return 0;
}

void hci_rx_thread(void)
{
    static uint8_t socket_buffer[4+65535]; /* Maximum is for ACL packet */

    for (;;) {
        int ret = hci_rx_read_from_socket(socket_buffer, 1);
        if (ret != 0) {
            printf(KCYN"%s Failed to read HCI packet type\n"KRST, __func__); flush_file(stdout);
            sleep(MS_TO_CLOCK(50)); /* Avoid spamming the logs too much */
            //TODO: in such a case, socket/HCI needs reset!
            continue;
        }

        switch (socket_buffer[0]) {
            case HCI_PKT_EVT:;
                hci_rx_handle_evt_packet(socket_buffer+1);
                break;
            case HCI_PKT_ACL:
                hci_rx_handle_acl_packet(socket_buffer+1);
                break;
            default:
                printf(KCYN"%s @socket_recv: unknown packet type %02x\n"KRST, __func__, socket_buffer[0]); flush_file(stdout);
                break;
        }
    }
}

/* Called by application to initialize transport structures */
void
hci_transport_init(void)
{
    int rc;

    spinlock_init(&socket_spinlock);

    trans_buf_cmd_allocd = 0;

    rc = os_mempool_init(&trans_buf_acl_pool, MYNEWT_VAL(BLE_ACL_BUF_COUNT),
                                ACL_POOL_BLOCK_SIZE, trans_buf_acl_pool_buf,
                                "dummy_hci_acl_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = os_mbuf_pool_init(&trans_buf_acl_mbuf_pool, &trans_buf_acl_pool,
                                ACL_POOL_BLOCK_SIZE,
                                MYNEWT_VAL(BLE_ACL_BUF_COUNT));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = os_mempool_init(&trans_buf_evt_hi_pool,
                                MYNEWT_VAL(BLE_HCI_EVT_HI_BUF_COUNT),
                                MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE),
                                trans_buf_evt_hi_pool_buf,
                                "dummy_hci_hci_evt_hi_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = os_mempool_init(&trans_buf_evt_lo_pool,
                                MYNEWT_VAL(BLE_HCI_EVT_LO_BUF_COUNT),
                                MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE),
                                trans_buf_evt_lo_pool_buf,
                                "dummy_hci_hci_evt_lo_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = ble_hci_trans_reset();
    SYSINIT_PANIC_ASSERT(rc == 0);

    return;
}
