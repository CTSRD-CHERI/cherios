/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/


#include "cheric.h"
#include "ff.h"
#include "assert.h"
#include "virtioblk.h"
#include "diskio.h"		/* FatFs lower layer API */
//#include "usbdisk.h"	/* Example: Header file of existing USB MSD control module */
//#include "atadrive.h"	/* Example: Header file of existing ATA harddisk control module */
//#include "sdcard.h"		/* Example: Header file of existing MMC/SDC contorl module */

/* Definitions of physical drive number for each drive */
#define ATA		0	/* Example: Map ATA harddisk to physical drive 0 */
#define MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define USB		2	/* Example: Map USB MSD to physical drive 2 */

const size_t sector_sz = 512;


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
		__unused BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	assert(pdrv == 0);
	return (DSTATUS)virtio_blk_status();
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
		__unused BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	assert(pdrv == 0);
	int vret = virtio_blk_init();
	if(vret != 0) {
		return STA_NODISK;
	}
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	__unused BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,		/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	assert(pdrv == 0);
	//printf("%s %ld %u\n", __func__, sector, count);
	for(size_t i=0; i<count; i++) {
		int ret = virtio_read(buff+(sector_sz*i), sector+i);
		if(ret) {
			assert(0);
			return RES_ERROR;
		}
	}
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	__unused BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	assert(pdrv == 0);
	//printf("%s %ld %u\n", __func__, sector, count);
	for(size_t i=0; i<count; i++) {
		int ret = virtio_write(buff+(sector_sz*i), sector+i);
		if(ret) {
			assert(0);
			return RES_ERROR;
		}
	}
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	__unused BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res = RES_PARERR;
	DWORD * buf = (DWORD *)buff;

	assert(pdrv == 0);
	//printf("%s: %d\n", __func__, cmd);
	switch(cmd) {
		case CTRL_SYNC:{}
			res = RES_OK;
			break;
		case GET_SECTOR_COUNT:{}
			size_t disk_sz = virtio_blk_size();
			*buf = disk_sz;
			res = RES_OK;
			break;
		case GET_BLOCK_SIZE:{}
			*buf = 1;
			res = RES_OK;
			break;
		default:
			assert(0);
	}

	return res;
}

