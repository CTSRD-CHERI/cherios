/*-
 * Copyright (c) 2020 Hadrien Barral
 * All rights reserved.
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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "os/os.h"
#include "log/log.h"
#include "nimble/nimble_port.h"
#include "stdio.h"
#include "syscalls.h"
#include "sysinit/sysinit.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
//#include "services/ans/ble_svc_ans.h"
//#include "services/ias/ble_svc_ias.h"
//#include "services/lls/ble_svc_lls.h"
//#include "services/tps/ble_svc_tps.h"

#include "ble_demo.h"

/* TODOX: put this in .h */
void ble_store_ram_init(void);
void nimble_host_task(void *param);
void hci_transport_init(void);
void hci_rx_thread(void);

static struct ble_npl_task s_task_host;

static void *ble_hci_rx_sock_task(void *param)
{
    (void)param;
    hci_rx_thread();
    return NULL;
}

static void *ble_host_task(void *param)
{
    (void)param;
    nimble_host_task(NULL);
    return NULL;
}


int main(void)
{
    hci_transport_init();
    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();
    //ble_svc_ans_init();
    //ble_svc_ias_init();
    //ble_svc_lls_init();
    //ble_svc_tps_init();

    ble_store_ram_init();

    uint8_t task_default_priority       = UINT8_C(1);
    ble_npl_stack_t *task_default_stack = NULL;
    uint16_t task_default_stack_size    = UINT8_C(400);

    /* Create a task to handle incoming frames from the BLE controller */
    static struct ble_npl_task s_task_hci_rx;
    ble_npl_task_init(&s_task_hci_rx, "hci_rx_sock", ble_hci_rx_sock_task,
                      NULL, task_default_priority, BLE_NPL_TIME_FOREVER,
                      task_default_stack, task_default_stack_size);

    /* Create a task to handles default the event queue for host stack */
    ble_npl_task_init(&s_task_host, "ble_host", ble_host_task,
                      NULL, task_default_priority, BLE_NPL_TIME_FOREVER,
                      task_default_stack, task_default_stack_size);

    bledemo_run();
	return 0;
}
