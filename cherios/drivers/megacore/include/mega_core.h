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
#ifndef FASTBOOT_MEGA_CORE_H
#define FASTBOOT_MEGA_CORE_H

#include "cdefs.h"

#ifdef SGDMA
#define MEGA_CORE_BASE_0 0x80007000         // 1024
#else
#define MEGA_CORE_BASE_0 0x7f007000         // 1024
#endif


#define MEGA_CORE_IRQ_RECV_0  1
#define MEGA_CORE_IRQ_TRAN_0  2
#define MEGA_CORE_BASE_1 0x7f005000
#define MEGA_CORE_IRQ_RECV_1  12
#define MEGA_CORE_IRQ_TRAN_1  11
#define MEGA_CORE_SIZE   0x600


#define MEGA_CORE_MAC_TRAN  0x400           // 8
#define MEGA_CORE_MAC_TRAN_CNTRL 0x420      // 32
#define MEGA_CORE_MAC_RECV  0x500           // 8
#define MEGA_CORE_MAC_RECV_CNTRL 0x520      // 32

typedef uint32_t MAC_DWORD; // This came from an intel doc so I assume...

typedef struct ALTERA_FIFO {
    volatile MAC_DWORD symbols;
    volatile MAC_DWORD metadata;
    volatile MAC_DWORD pad[0x6];
    volatile MAC_DWORD ctrl_fill_level;
    volatile MAC_DWORD ctrl_i_status;
    volatile MAC_DWORD ctrl_i_event;
    volatile MAC_DWORD ctrl_ie;
    volatile MAC_DWORD ctrl_almostfull;
    volatile MAC_DWORD ctrl_almostempty;
    volatile MAC_DWORD pad2[0x32];
} ALTERA_FIFO;

typedef struct mac_control {
    struct {
        volatile MAC_DWORD rev;
        volatile MAC_DWORD scratch;
        volatile MAC_DWORD command_config;
        #define COMMAND_CONFIG_TX_ENA           0
        #define COMMAND_CONFIG_RX_ENA           1
        #define COMMAND_CONFIG_XON_GEN          2
        #define COMMAND_CONFIG_ETH_SPEED        3
        #define COMMAND_CONFIG_PROMIS_EN        4
        #define COMMAND_CONFIG_PAD_EN           5
        #define COMMAND_CONFIG_CRC_FWD          6
        #define COMMAND_CONFIG_PAUSE_FWD        7
        #define COMMAND_CONFIG_PAUSE_IGNORE     8
        #define COMMAND_CONFIG_TX_ADDR_INS      9
        #define COMMAND_CONFIG_HD_ENA           10
        #define COMMAND_CONFIG_EXCESS_COL       11
        #define COMMAND_CONFIG_LATE_COL         12
        #define COMMAND_CONFIG_SW_RESET         13
        #define COMMAND_CONFIG_MHASH_SEL        14
        #define COMMAND_CONFIG_LOOP_ENA         15
        #define COMMAND_CONFIG_TX_ADDR_SEL      16 // (3 of em)
        #define COMMAND_CONFIG_MAGIC_ENA        19
        #define COMMAND_CONFIG_SLEEP            20
        #define COMMAND_CONFIG_WAKEUP           21
        #define COMMAND_CONFIG_XOFF_GEN         22
        #define COMMAND_CONFIG_CNTRL_FRM_ENA    23
        #define COMMAND_CONFIG_NO_LGTH_CHECK    24
        #define COMMAND_CONFIG_ENA_10           25
        #define COMMAND_CONFIG_RX_ERR_DISC      26
        #define COMMAND_CONFIG_DISABLE_READ_TIMEOUT 27
        #define COMMAND_CONFIG_RESERVED         28
        #define COMMAND_CONFIG_CNT_RESET        31

#define CC_N_MASK(X) (1 << ((X & 0x7) + (24 - (X & ~0x7))))
#define CC_MASK(X) (1 << X)

#define NTOH32(X) (((X & 0xFF) << 24) | ((X & 0xFF00) << 8) | ((X & 0xFF0000) >> 8) | ((X & 0xFF000000) >> 24))

        volatile MAC_DWORD mac_0;
        volatile MAC_DWORD mac_1;
        volatile MAC_DWORD frm_length;
        volatile MAC_DWORD pause_quant;
        volatile MAC_DWORD rx_section_empty;
        volatile MAC_DWORD rx_section_full;
        volatile MAC_DWORD tx_section_empty;
        volatile MAC_DWORD tx_section_full;
        volatile MAC_DWORD rx_almost_empty;
        volatile MAC_DWORD rx_almost_full;
        volatile MAC_DWORD tx_almost_empty;
        volatile MAC_DWORD tx_almost_full;
        volatile MAC_DWORD mdio_addr0;
        volatile MAC_DWORD mdio_addr1;
        volatile MAC_DWORD holdoff_quant;
        volatile MAC_DWORD reserved[5];
        volatile MAC_DWORD tx_ipg_length;
    } base_config; // volatile MAC_DWORD [0x18];

    struct {
      volatile MAC_DWORD aMacID[2];
      volatile MAC_DWORD aFramesTransmittedOK;
      volatile MAC_DWORD aFramesReceivedOK;
      volatile MAC_DWORD aFrameCheckSequenceErrors;
      volatile MAC_DWORD aALignmentErrors;
      volatile MAC_DWORD aOctetsTransmittedOK;
      volatile MAC_DWORD aOctetsReceivedOK;
      volatile MAC_DWORD aTxPAUSEMACCtrlFrames;
      volatile MAC_DWORD aRxPAUSEMACCtrlFrames;
      volatile MAC_DWORD ifInErrors;
      volatile MAC_DWORD ifOutErrors;
      volatile MAC_DWORD ifInUcastPkts;
      volatile MAC_DWORD ifInMulticastPkt;
      volatile MAC_DWORD ifInBroadcastPkt;
      volatile MAC_DWORD ifOutDiscards;
      volatile MAC_DWORD ifOutUcastPkts;
      volatile MAC_DWORD ifOutMulticastPkts;
      volatile MAC_DWORD ifOutBroadcastPkts;
      volatile MAC_DWORD etherStatsDropEvents;
      volatile MAC_DWORD etherStatsOctets;
      volatile MAC_DWORD etherStatsPkts;
      volatile MAC_DWORD etherStatsUndersizePkts;
      volatile MAC_DWORD etherStatsOversizePkts;
      volatile MAC_DWORD etherStatsPkts64Octets;
      volatile MAC_DWORD etherStatsPkts65to127Octets;
      volatile MAC_DWORD etherStats128to255Octets;
      volatile MAC_DWORD etherStats256to511Octets;
      volatile MAC_DWORD etherStats512to1023Octets;
      volatile MAC_DWORD etherStats1024to1518Octets;
      volatile MAC_DWORD etherStats1519toXOctets;
      volatile MAC_DWORD etherStatsJabber;
      volatile MAC_DWORD etherStatsFragments;
      volatile MAC_DWORD reserved;
    } stat_ctrs; // volatile MAC_DWORD [0x22]

    volatile MAC_DWORD tx_cmd_stat;
    volatile MAC_DWORD rx_cmd_stat;

#define CMD_STAT_OMIT_CRC   17  // 1 = omit
#define CMD_STAT_TX_SHIFT16 18  // 1 = align 32 bits
#define CMD_STAT_RX_SHIFT16 25  // 1 = align 32 bits

    struct {
      volatile MAC_DWORD msb_aOctetsTransmittedOK;
      volatile MAC_DWORD msb_aOctetsReceivedOK;
      volatile MAC_DWORD nsv_etherStatsOctets;
    } stat_ctrs_ext;

    //0x3E
    MAC_DWORD pad0[2];

    //0x40..0xC0

    MAC_DWORD pad1[0x80];

    MAC_DWORD smacs[8];

    // 0xC8

#ifndef SGDMA
    volatile MAC_DWORD pad2[(MEGA_CORE_MAC_TRAN/4)-0xC9];

    ALTERA_FIFO tran_fifo;

    ALTERA_FIFO recv_fifo;
#endif

} mac_control;

_Static_assert(__offsetof(mac_control,tx_cmd_stat) == (0x3A * 4), "Alignment check");
_Static_assert(__offsetof(mac_control,rx_cmd_stat) == (0x3B * 4), "Alignment check");


#ifndef SGDMA
_Static_assert(__offsetof(mac_control,tran_fifo) == (MEGA_CORE_MAC_TRAN), "Alignment check");
_Static_assert(__offsetof(mac_control,recv_fifo) == (MEGA_CORE_MAC_RECV), "Alignment check");
#endif

#endif //FASTBOOT_MEGA_CORE_H
