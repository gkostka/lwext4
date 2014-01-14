/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include <hw_init.h>
#include <usbh_msc_core.h>
#include <usbh_usr.h>

/**@brief   Block size.*/
#define USB_MSC_BLOCK_SIZE          512

/**@brief   MBR_block ID*/
#define MBR_BLOCK_ID                0
#define MBR_PART_TABLE_OFF          446

extern USB_OTG_CORE_HANDLE          USB_OTG_Core;
extern USBH_HOST                    USB_Host;

struct part_tab_entry {
    uint8_t  status;
    uint8_t  chs1[3];
    uint8_t  type;
    uint8_t  chs2[3];
    uint32_t first_lba;
    uint32_t sectors;
}__attribute__((packed));

/**@brief   Partition block offset*/
static uint32_t part_offset;

/**********************BLOCKDEV INTERFACE**************************************/
static int usb_msc_open(struct ext4_blockdev *bdev);
static int usb_msc_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int usb_msc_bwrite(struct ext4_blockdev *bdev, const void *buf,
    uint64_t blk_id, uint32_t blk_cnt);
static int usb_msc_close(struct  ext4_blockdev *bdev);


/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(
    _usb_msc,
    USB_MSC_BLOCK_SIZE,
    0,
    usb_msc_open,
    usb_msc_bread,
    usb_msc_bwrite,
    usb_msc_close
);

/******************************************************************************/
EXT4_BCACHE_STATIC_INSTANCE(_usb_msc_cache, CONFIG_BLOCK_DEV_CACHE_SIZE, EXT_LOGICAL_BLOCK_SIZE);

/******************************************************************************/

static int usb_msc_open(struct ext4_blockdev *bdev)
{
    (void)bdev;

    static uint8_t mbr[512];
    struct part_tab_entry *part0;
    uint8_t status;

    if(!hw_usb_connected())
        return EIO;
    do
    {
        status = USBH_MSC_Read10(&USB_OTG_Core, mbr, 0, USB_MSC_BLOCK_SIZE);
        USBH_MSC_HandleBOTXfer(&USB_OTG_Core ,&USB_Host);

        if(!hw_usb_connected())
            return EIO;

    }
    while(status == USBH_MSC_BUSY );

    if(status != USBH_MSC_OK)
        return EIO;

    part0 = (struct part_tab_entry *)(mbr + MBR_PART_TABLE_OFF);

    part_offset = part0->first_lba;
    _usb_msc.ph_bcnt = USBH_MSC_Param.MSCapacity;

    return hw_usb_connected() ? EOK : EIO;
}

static int usb_msc_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt)
{
    uint8_t status;

    if(!hw_usb_connected())
        return EIO;

    do
    {
        status = USBH_MSC_Read10(&USB_OTG_Core, buf, blk_id + part_offset, _usb_msc.ph_bsize * blk_cnt);
        USBH_MSC_HandleBOTXfer(&USB_OTG_Core ,&USB_Host);

        if(!hw_usb_connected())
            return EIO;
    }
    while(status == USBH_MSC_BUSY );

    if(status != USBH_MSC_OK)
        return EIO;

    return EOK;

}

static int usb_msc_bwrite(struct ext4_blockdev *bdev, const void *buf,
    uint64_t blk_id, uint32_t blk_cnt)
{
    uint8_t status;

    if(!hw_usb_connected())
        return EIO;

    do
    {
        status = USBH_MSC_Write10(&USB_OTG_Core, (void *)buf, blk_id + part_offset, _usb_msc.ph_bsize * blk_cnt);
        USBH_MSC_HandleBOTXfer(&USB_OTG_Core ,&USB_Host);

        if(!hw_usb_connected())
            return EIO;
    }
    while(status == USBH_MSC_BUSY );

    if(status != USBH_MSC_OK)
        return EIO;

    return EOK;
}

static int usb_msc_close(struct  ext4_blockdev *bdev)
{
    (void)bdev;
    return EOK;
}

/******************************************************************************/

struct ext4_bcache*   ext4_usb_msc_cache_get(void)
{
    return &_usb_msc_cache;
}


struct ext4_blockdev* ext4_usb_msc_get(void)
{
    return &_usb_msc;
}

