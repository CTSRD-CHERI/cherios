/*-
 * Copyright (c) 2019 Lawrence Esswood
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
#include "syscalls.h"
#include "sys/types.h"
#include "stdlib.h"

#define MAX_TRACK 100
#define MAX_DISPLAY 30

act_info_t* info_global;

int cmp(const void* a, const void* b) {
    int64_t diff = (int64_t)(info_global[*(size_t*)b].had_time_epoch - info_global[*(size_t*)a].had_time_epoch);
    return diff > 0 ? 1 : (int)(diff >> 32);
}

#define ANSI_ESC    "\x1B"
#define ANSI_CURSOR_SAVE ANSI_ESC "7"
#define ANSI_CURSOR_RESTORE ANSI_ESC "8"

#define ANSI_ESC_C "\x1B["
#define ANSI_CURSOR_PREV "F"
#define ANSI_CURSOR_SET_WINDOW "r"
#define ANSI_CURSOR_HOME "H"
#define ANSI_SET_CURSOR  "H"
#define ANSI_CLEAR_UP "1J"
#define ANSI_CLEAR_ALL "2J"

#define CTRL_START ANSI_CURSOR_SAVE ANSI_ESC_C ANSI_CURSOR_HOME
#define CTRL_END ANSI_CURSOR_RESTORE

#define H1 "|----------------------------------------------------------------------------|\n"
#define H2 "|      Name      |Total Time| Time |CPU| Switches | Sent | Recv |   Status   |\n"
#define H3 "|----------------+----------+------+---+----------+------+------+------------|\n"
#define H4 "                                                                              \n"

#define LL (sizeof(H1)-1)
#define LC0 (sizeof(CTRL_START)-1)
#define LC1 (sizeof(CTRL_END)-1)

#define LTOTAL (LL * (MAX_DISPLAY + 4)) + LC0 + LC1



int main(register_t arg, capability carg) {
    // This just displays some stats every few seconds
    char table_buffer[LTOTAL];
    act_info_t info[MAX_TRACK];
    info_global = info;
    size_t order[MAX_TRACK];

    // setup the display

    // Set window


    printf(ANSI_CURSOR_SAVE
            ANSI_ESC_C "%d;" ANSI_CURSOR_SET_WINDOW
           ANSI_ESC_C "%d;0" ANSI_SET_CURSOR
           ANSI_ESC_C ANSI_CLEAR_UP
           ANSI_CURSOR_RESTORE
            , 2 + MAX_DISPLAY + 4, 2 + MAX_DISPLAY + 4);
    socket_flush_drb(stdout);

    // init top of table
    memcpy(table_buffer, CTRL_START, LC0);
    memcpy(table_buffer+LC0,H1,LL);
    memcpy(table_buffer+LC0+LL,H2,LL);
    memcpy(table_buffer+LC0+(LL*2),H3,LL);

    while(1) {
        // get info about all the activations
        size_t n_acts = 0;
        uint64_t total_time = 0;
        act_control_kt act = syscall_actlist_first();

        while(act != NULL) {
            order[n_acts] = n_acts;
            syscall_act_info(act, &info[n_acts]);
            total_time += info[n_acts].had_time_epoch;
            n_acts++;
            act = syscall_actlist_next(act);
        }

        if(total_time == 0) total_time = 1;

        syscall_info_epoch();

        // sort by time had
        qsort(order, n_acts, sizeof(size_t), &cmp);

        // now pretty print

        char* buf = table_buffer + LC0 + (LL * 3);

        for(size_t i = 0; i != n_acts && i != MAX_DISPLAY; i++) {
            act_info_t* act_info = &info[order[i]];

            char status[] = "Block:XXXXXX";
            size_t flags = 6;

            char* sched_str;

            switch (act_info->sched_status) {
                case sched_runnable:
                    sched_str = "Runnable";
                    break;
                case sched_running:
                    sched_str = "Running";
                    break;
                case sched_terminated:
                    sched_str = "Terminated";
                    break;
                default:
#define ADD_F(flag, c) if(act_info->sched_status & flag) status[flags++] = c
                    ADD_F(sched_waiting, 'M');
                    ADD_F(sched_sync_block, 'R');
                    ADD_F(sched_sem, 'S');
                    ADD_F(sched_wait_notify,'N');
                    ADD_F(sched_wait_commit, 'C');
                    status[flags] = '\0';
                    sched_str = status;
            }

            uint64_t per_k = (act_info->had_time_epoch * 1000) / total_time;
            uint64_t per_c = per_k / 10;
            uint64_t decimal = per_k % 10;
            uint64_t total = CLOCK_TO_MS(act_info->had_time) / 1000;
            snprintf(buf, LL + 1, "|%16s|%9lds|%3ld.%1ld%%|%3d|%10ld|%6ld|%6ld|%12s|\n",
                   act_info->name, total, per_c, decimal, act_info->cpu, act_info->switches,
                   act_info->sent_n, act_info->received_n, sched_str);
            buf += LL;
        }

        memcpy(buf, H1, LL);
        buf += LL;

        if(n_acts < MAX_DISPLAY) {
            for(size_t i = n_acts; i != MAX_DISPLAY; i++) {
                memcpy(buf, H4, LL);
                buf +=LL;
            }
        }

        // init end of table

        memcpy(table_buffer + LC0 + (LL * (MAX_DISPLAY + 4)), CTRL_END, LC1);

        socket_internal_requester_space_wait(stdout->write.push_writer,1,0,0);
        socket_internal_request_ind(stdout->write.push_writer, table_buffer, LTOTAL, 0);

        sleep(MS_TO_CLOCK(5 * 1000));
    }
}
