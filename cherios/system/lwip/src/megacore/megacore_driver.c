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

#include "mega_core.h"
#include "lwip_driver.h"
#include "mman.h"

int lwip_driver_init(net_session* session) {
    cap_pair pair;

    // Get MMIO
    get_physical_capability(own_mop, &pair, MEGA_CORE_BASE_0, MEGA_CORE_SIZE, 1, 0);

    session->mmio = (lwip_driver_mmio_t*)pair.data;

    // Register IRQ

    mac_control* ctrl = session->mmio;

    // Use scratch register to check we have mapped the right place
    MAC_DWORD test = 0xfefe;
    ctrl->base_config.scratch = test;
    if(test != ctrl->base_config.scratch) return -1;


    // Do an initial reset to clear everything
    ctrl->base_config.command_config = CC_N_MASK(COMMAND_CONFIG_SW_RESET) | CC_N_MASK(COMMAND_CONFIG_CNT_RESET);

    int retry = 0;

    CHERI_PRINT_CAP(ctrl);

    // Wait for it to happen (should only take a few cycles so we don't sleep)
    while(ctrl->base_config.command_config & CC_N_MASK(COMMAND_CONFIG_SW_RESET)) {
        if((++retry & 0xF) == 0) {
            printf("LWIP still waiting for ethernet reset... %d\n", retry);
            sleep(MS_TO_CLOCK(5000));
        }
    };

    MAC_DWORD mac0 = (((((session->mac[0] << 8) | session->mac[1]) << 8) | session->mac[2]) << 8) | session->mac[3];
    MAC_DWORD mac1 = ((session->mac[4] << 8) | session->mac[5]) << 16;

    ctrl->base_config.mac_0 = mac0;
    ctrl->base_config.mac_1 = mac1;

    for(size_t i = 0; i < 4; i++) {
        ctrl->smacs[(2*i) + 0] = mac0;
        ctrl->smacs[(2*i) + 1] = mac1;
    }

    MAC_DWORD config = 0;

    // Also for 32bit align

    config = CC_N_MASK(COMMAND_CONFIG_PAD_EN);

    ctrl->base_config.command_config = config;

    // These will shift alignment to achieve 32 bit align for ethernet headers

    MAC_DWORD tx_cmd = CC_N_MASK(CMD_STAT_TX_SHIFT16);
    MAC_DWORD rx_cmd = CC_N_MASK(CMD_STAT_RX_SHIFT16);

    ctrl->tx_cmd_stat = tx_cmd;
    ctrl->rx_cmd_stat = rx_cmd;

    // Finally enable TX/RX

    config |= CC_N_MASK(COMMAND_CONFIG_TX_ENA) | CC_N_MASK(COMMAND_CONFIG_RX_ENA);

    ctrl->base_config.command_config = config;

    MAC_DWORD got_config = ctrl->base_config.command_config;
    MAC_DWORD tx_cmd_got = ctrl->tx_cmd_stat;
    MAC_DWORD rx_cmd_got = ctrl->rx_cmd_stat;

    // Check we got the config options we asked for
    if(config != got_config || tx_cmd_got != tx_cmd || rx_cmd_got != rx_cmd) return -2;

    int res = altera_transport_init(session);

    if(res < 0) return res;
    return 0;
}
