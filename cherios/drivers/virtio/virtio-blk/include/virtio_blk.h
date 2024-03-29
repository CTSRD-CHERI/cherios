#ifndef _LINUX_VIRTIO_BLK_H
#define _LINUX_VIRTIO_BLK_H
/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */

#include "virtio_types.h"
#include "virtio.h"

/* Feature bits */
#define VIRTIO_BLK_F_SIZE_MAX	1	/* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX	2	/* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY	4	/* Legacy geometry available  */
#define VIRTIO_BLK_F_RO		5	/* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE	6	/* Block size of disk is available*/
#define VIRTIO_BLK_F_TOPOLOGY	10	/* Topology information is available */
#define VIRTIO_BLK_F_MQ		12	/* support more than one vq */

#define VIRTIO_BLK_ID_BYTES	20	/* ID string length */

// QEMU is all sorts of broken.
// It appears that the 64 bit field is broken into two 32-bit fields (big-endian)
// and then the two 32 bit fields are then ordered depending on the virtio ordering
// (little or host order depending on version).

struct virtio_blk_config {
	/* The capacity (in 512-byte sectors). */
	uint64_t capacity;
	/* The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX) */
	uint32_t size_max;
	/* The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX) */
	uint32_t seg_max;
	/* geometry of the device (if VIRTIO_BLK_F_GEOMETRY) */
	struct virtio_blk_geometry {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;

	/* block size of device (if VIRTIO_BLK_F_BLK_SIZE) */
	uint32_t blk_size;

	/* the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY  */
	/* exponent for physical block per logical block. */
	uint8_t physical_block_exp;
	/* alignment offset in logical blocks. */
	uint8_t alignment_offset;
	/* minimum I/O size without performance penalty in logical blocks. */
	uint16_t min_io_size;
	/* optimal sustained I/O size in logical blocks. */
	uint32_t opt_io_size;

	/* writeback mode (if VIRTIO_BLK_F_CONFIG_WCE) */
	uint8_t wce;
	uint8_t unused;

	/* number of vqs, only available when VIRTIO_BLK_F_MQ is set */
	uint16_t num_queues;
};

#if (VIRTIO_IS_LITTLE_ENDIAN)
    #define VIRTIO_BLKCONFIG_SWAP_U64(X)                                      \
        ({uint64_t Y = X;                                               \
        (((uint64_t)__builtin_bswap32((uint32_t)((Y) >> 32))) << 32) |  \
        (uint64_t)__builtin_bswap32((uint32_t)(Y) & 0xFFFFFFFFU);})
#else
    #define VIRTIO_BLKCONFIG_SWAP_U64(X) (X)
#endif



/* These two define direction. */
#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1

/* Cache flush command */
#define VIRTIO_BLK_T_FLUSH	4

/* Get device ID command */
#define VIRTIO_BLK_T_GET_ID    8

/*
 * This comes first in the read scatter-gather list.
 * For legacy virtio, if VIRTIO_F_ANY_LAYOUT is not negotiated,
 * this is the first element of the read scatter-gather list.
 */
struct virtio_blk_outhdr {
	/* VIRTIO_BLK_T* */
	__virtio32 type;
	/* io priority. */
	__virtio32 ioprio;
	/* Sector (ie. 512 byte offset) */
	__virtio64 sector;
};

struct virtio_blk_inhdr
{
    unsigned char status;
};

/* And this is the final byte of the write scatter-gather list. */
#define VIRTIO_BLK_S_OK		0
#define VIRTIO_BLK_S_IOERR	1
#define VIRTIO_BLK_S_UNSUPP	2
#endif /* _LINUX_VIRTIO_BLK_H */
