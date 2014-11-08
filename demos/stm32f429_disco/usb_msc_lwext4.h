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
#ifndef USB_MSC_LWEXT4_H_
#define USB_MSC_LWEXT4_H_

#include <ext4_config.h>
#include <ext4_blockdev.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief   USB MSC cache get.*/
struct ext4_bcache*   ext4_usb_msc_cache_get(void);

/**@brief   USB MSC blockdev get.*/
struct ext4_blockdev* ext4_usb_msc_get(void);



struct ext4_io_stats {
    float io_read;
    float io_write;
    float cpu;
};

void ext4_io_timings_clear(void);
const struct ext4_io_stats * ext4_io_timings_get(uint32_t time_sum_ms);




#endif /* USB_MSC_LWEXT4_H_ */
