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

#include <alteraSD.h>
#include "cheric.h"
#include "sockets.h"
#include "alteraSD.h"
#include "stdlib.h"
#include "misc.h"
#include "thread.h"
#include "namespace.h"
#include "endian.h"

#define MAX_SOCKS           4
#define SECTOR_SIZE         512

enum session_state {
    created,
    initted,
    destroyed,
};

typedef struct session_sock {
    struct session_t* session;
    uni_dir_socket_fulfiller ff;
    size_t sector;
    size_t sector_prog;
} session_sock;

typedef struct session_t {
    enum session_state state;
    altera_sd_mmio* mmio;
    size_t size;
} session_t;

session_sock socks[MAX_SOCKS];
size_t socks_count;

capability sealer;

static session_t* seal_session(session_t* session) {
    return (session_t*)cheri_seal(session, sealer);
}

static session_t* unseal_session(session_t* session) {
    session = cheri_unseal_2(session, sealer);
    if(session == NULL) return NULL;
    enum session_state state;
    ENUM_VMEM_SAFE_DEREFERENCE(&session->state, state, destroyed);
    return (state == destroyed) ? NULL : session;
}

session_t* new_session(altera_sd_mmio * mmio_cap) {
    session_t* session = (session_t*)malloc(sizeof(session_t));
    session->mmio = mmio_cap;
    session->state = created;
    return seal_session(session);
}

int vblk_init(session_t* session) {
    // TODO
    session = unseal_session(session);

    if(session == NULL || session->state != created) return -1;

    altera_sd_mmio* mmio = session->mmio;

    uint16_t present = (mmio->asr & HTOLE16(ALTERA_SDCARD_ASR_CARDPRESENT));

    // First wait for the card to be present

    while(!present) {
        printf("Waiting for SD to be present. asr: %x\n", mmio->asr);
        sleep(ALTERA_SDCARD_TIMEOUT_IDLE);
        present = (mmio->asr & HTOLE16(ALTERA_SDCARD_ASR_CARDPRESENT));
    }

    // Then configure. Need to get to idle state.

    uint16_t csd[8];

    for(size_t i = 0; i != 8; i++) {
        csd[i] = mmio->csd[i];
    }

    uint8_t* as_bytes = (uint8_t*)csd;
    uint8_t structure = as_bytes[ALTERA_SDCARD_CSD_STRUCTURE_BYTE];

    structure &= ALTERA_SDCARD_CSD_STRUCTURE_MASK;
    structure >>= ALTERA_SDCARD_CSD_STRUCTURE_RSHIFT;

    // Only support V1.0

    if(structure != 0) return -3;
    // TODO need to init size


    uint64_t c_size, c_size_mult, read_bl_len;
    uint8_t byte0, byte1, byte2;

    /*-
     * Compute card capacity per SD Card interface description as follows:
     *
     *   Memory capacity = BLOCKNR * BLOCK_LEN
     *
     * Where:
     *
     *   BLOCKNR = (C_SIZE + 1) * MULT
     *   MULT = 2^(C_SIZE_MULT+2)
     *   BLOCK_LEN = 2^READ_BL_LEN
     */
    read_bl_len = as_bytes[ALTERA_SDCARD_CSD_READ_BL_LEN_BYTE];
    read_bl_len &= ALTERA_SDCARD_CSD_READ_BL_LEN_MASK;

    byte0 = as_bytes[ALTERA_SDCARD_CSD_C_SIZE_BYTE0];
    byte0 &= ALTERA_SDCARD_CSD_C_SIZE_MASK0;
    byte1 = as_bytes[ALTERA_SDCARD_CSD_C_SIZE_BYTE1];
    byte2 = as_bytes[ALTERA_SDCARD_CSD_C_SIZE_BYTE2];
    byte2 &= ALTERA_SDCARD_CSD_C_SIZE_MASK2;
    c_size = (byte0 >> ALTERA_SDCARD_CSD_C_SIZE_RSHIFT0) |
             (byte1 << ALTERA_SDCARD_CSD_C_SIZE_LSHIFT1) |
             (byte2 << ALTERA_SDCARD_CSD_C_SIZE_LSHIFT2);

    byte0 = as_bytes[ALTERA_SDCARD_CSD_C_SIZE_MULT_BYTE0];
    byte0 &= ALTERA_SDCARD_CSD_C_SIZE_MULT_MASK0;
    byte1 = as_bytes[ALTERA_SDCARD_CSD_C_SIZE_MULT_BYTE1];
    byte1 &= ALTERA_SDCARD_CSD_C_SIZE_MULT_MASK1;
    c_size_mult = (byte0 >> ALTERA_SDCARD_CSD_C_SIZE_MULT_RSHIFT0) |
                  (byte1 << ALTERA_SDCARD_CSD_C_SIZE_MULT_LSHIFT1);

    /*
     * If we're just getting back zero's, mark the card as bad, even
     * though it could just mean a Very Small Disk Indeed.
     */

    if (c_size == 0 && c_size_mult == 0 && read_bl_len == 0) return -4;

    session->size = (c_size + 1) * (1 << (c_size_mult + 2)) *
                       (1 << read_bl_len);

    session->state = initted;

    return 0;
}

int new_socket(session_t* session, uni_dir_socket_requester* requester, enum socket_connect_type type) {
    session = unseal_session(session);

    assert(session != NULL);
    assert(session->state == initted);

    if(socks_count == MAX_SOCKS) return -1;

    session_sock* ss = &socks[socks_count];

    uint8_t sock_type;
    if(type == CONNECT_PUSH_WRITE) {
        sock_type = SOCK_TYPE_PUSH;
    } else if(type == CONNECT_PULL_READ) {
        sock_type = SOCK_TYPE_PULL;
    } else return -1;

    ssize_t res;
    if((res = socket_internal_fulfiller_init(&ss->ff, sock_type)) < 0) return (int)res;
    if((res = socket_internal_fulfiller_connect(&ss->ff, requester)) < 0) return (int)res;

    ss->session = session;
    ss->sector = 0;

    socks_count++;

    return 0;
}

static ssize_t full_oob(capability arg, request_t* request, uint64_t offset, uint64_t partial_bytes, uint64_t length) {
    session_sock* ss = (session_sock*)arg;
    request_type_e req = request->type;

    if(req == REQUEST_SEEK) {
        assert(ss->sector_prog == 0);
        int64_t seek_offset = request->request.seek_desc.v.offset;
        int whence = request->request.seek_desc.v.whence;

        size_t target_offset;

        switch (whence) {
            case SEEK_CUR:
                target_offset = seek_offset + ss->sector;
                break;
            case SEEK_SET:
                if(seek_offset < 0) return E_OOB;
                target_offset = (size_t)seek_offset;
                break;
            case SEEK_END:
            default:
                return E_OOB;
        }

        ss->sector = target_offset;

        return length;
    } else if(req == REQUEST_FLUSH) {
        assert(0 && "TODO");
    }

    return E_OOB;
}

static int rw_block(altera_sd_mmio* mmio, size_t sector, int write) {

    uint16_t rr1;
    uint16_t cmd = write ? (HTOLE16((ALTERA_SDCARD_CMD_WRITE_BLOCK))) : (HTOLE16((ALTERA_SDCARD_CMD_READ_BLOCK)));

    size_t addr = (sector * SECTOR_SIZE);
    mmio->cmd_arg = HTOLE32(addr);
    mmio->cmd = cmd;

    while(mmio->asr & HTOLE16(ALTERA_SDCARD_ASR_CMDINPROGRESS)) {
        //sleep(ALTERA_SDCARD_TIMEOUT_IO);
    }

    uint16_t status = mmio->asr;

    if(!(status & HTOLE16(ALTERA_SDCARD_ASR_CARDPRESENT))) {
        assert(0 && "Card removed mid op\n");
    }

    if(!(status & (HTOLE16(ALTERA_SDCARD_ASR_CMDVALID)))) {
        assert(0);
    }

    if(status & (HTOLE16(ALTERA_SDCARD_ASR_CMDTIMEOUT))) {
        assert(0);
        // TODO should retry here
    }

    if(status & HTOLE16(ALTERA_SDCARD_ASR_CMDDATAERROR)) {
        // Always seem to get CRC fail for r and w
        rr1 = mmio->rr1 & HTOLE16(ALTERA_SDCARD_RR1_ERRORMASK &~ALTERA_SDCARD_RR1_COMMANDCRCFAILED);
        if(rr1) {
            printf("RR1: %x\n", rr1);
            assert(0);
        }
    }

    return 0;
}

static size_t memcpy_tmp(uint8_t* dst, uint8_t* src, size_t length) {
    for(size_t i = 0; i != length; i++) {
        dst[i] = src[i];
    }
    return length;
}

static ssize_t ful_ff(capability arg, char* buf, uint64_t offset, uint64_t length) {
    session_sock *ss = (session_sock *) arg;
    altera_sd_mmio* mmio = ss->session->mmio;
    int push = ss->ff.socket_type == SOCK_TYPE_PUSH;
    uint64_t written = 0;

    while(length != 0) {

        if(ss->sector_prog == 0) {

            // If this is a pull, then pull now
            if(!push) rw_block(mmio, ss->sector, 0);
        }

        uint64_t remains = SECTOR_SIZE - ss->sector_prog;
        uint64_t to_copy = length > remains ? remains : length;

        char* src =  push ? buf : (mmio->rxtx_buf + ss->sector_prog);
        char* dst =  push ?  (mmio->rxtx_buf + ss->sector_prog) : buf;

        memcpy((uint8_t *)dst, (uint8_t *)src, to_copy);

        length -=to_copy;
        ss->sector_prog+=to_copy;
        written +=to_copy;
        buf +=to_copy;

        if(ss->sector_prog == SECTOR_SIZE) {

            // If this a push, write this block

            if(push) rw_block(mmio, ss->sector, 1);

            ss->sector_prog = 0;
            ss->sector++;
        }
    }

    return written;
}

void vblk_interrupt(void* sealed_session, register_t a0, register_t irq) {
    // No interrupts for SD. Sad =(.
}

void handle_socket(session_sock* ss) {
    ssize_t bytes = socket_internal_requester_bytes_requested(ss->ff.requester);
    ssize_t res = socket_internal_fulfill_progress_bytes(&ss->ff, SOCK_INF, F_CHECK | F_PROGRESS | F_DONT_WAIT, &ful_ff, ss, 0, &full_oob);
    if(bytes != 0 && res == E_AGAIN) return; // Allow just OOBS
    assert_int_ex(-res, <=, 0);
    assert_int_ex(res & (SECTOR_SIZE-1), ==, 0);
    assert_int_ex(ss->sector_prog, ==, 0);
    return;
}

size_t vblk_size(session_t* session) {
    session = unseal_session(session);
    assert(session != NULL);
    assert(session->state == initted);
    return session->size / SECTOR_SIZE;
}

int vblk_read(session_t* session, void * buf, size_t sector) {
    session = unseal_session(session);
    assert(session != NULL);
    assert(session->state == initted);
    int res = rw_block(session->mmio, sector, 0);
    if(res != 0) return res;
    memcpy(buf, (uint8_t*)session->mmio->rxtx_buf, SECTOR_SIZE);
    return 0;
}

int vblk_write(session_t* session, void * buf, size_t sector) {
    session = unseal_session(session);
    assert(session != NULL);
    assert(session->state == initted);
    memcpy((uint8_t*)session->mmio->rxtx_buf, buf, SECTOR_SIZE);
    return rw_block(session->mmio, sector, 1);
}

uint8_t vblk_status(session_t* session) {
    session = unseal_session(session);
    assert(session != NULL);

    if(session->state != initted) return 1;

    uint16_t status = session->mmio->asr;

    return (status & HTOLE16(ALTERA_SDCARD_ASR_CARDPRESENT)) ? (uint8_t)0 : (uint8_t)2;
}

void handle_loop(void) {

    POLL_LOOP_START(sock_sleep, sock_event, 1)

        for(size_t i = 0; i < socks_count; i++) {
            session_sock* ss = &socks[i];
            POLL_ITEM_F(event, sock_sleep, sock_event, &ss->ff, POLL_IN, 0)
            if(event) {
                handle_socket(ss);
            }
        }

    POLL_LOOP_END(sock_sleep, sock_event, 1, 0);
}



void (*msg_methods[]) = {vblk_init, vblk_read, vblk_write, vblk_status, vblk_size, new_socket};
size_t msg_methods_nb = countof(msg_methods);
void (*ctrl_methods[]) = {NULL, new_session, NULL, vblk_interrupt};
size_t ctrl_methods_nb = countof(ctrl_methods);

int main(register_t arg, capability carg) {
    printf("AlteraSD: Hello World!\n");
    sealer = get_type_owned_by_process();
    namespace_register(namespace_num_virtio, act_self_ref);
    printf("AlteraSD: Going into poll loop\n");
    syscall_change_priority(act_self_ctrl, PRIO_HIGH);
    handle_loop();
}
