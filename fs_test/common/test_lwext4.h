/*
 * Copyright (c) 2014 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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

#ifndef TEST_LWEXT4_H_
#define TEST_LWEXT4_H_

#include <stdint.h>
#include <stdbool.h>
#include <ext4.h>

void test_lwext4_dir_ls(const char *path);
void test_lwext4_mp_stats(void);
void test_lwext4_block_stats(void);
bool test_lwext4_dir_test(int len);
bool test_lwext4_file_test(uint8_t *rw_buff, uint32_t rw_size, uint32_t rw_count);
void test_lwext4_cleanup(void);

bool test_lwext4_mount(struct ext4_blockdev *bdev, struct ext4_bcache *bcache);
bool test_lwext4_umount(void);

void tim_wait_ms(uint32_t v);

uint32_t tim_get_ms(void);
uint64_t tim_get_us(void);

struct ext4_io_stats {
	float io_read;
	float io_write;
	float cpu;
};

void io_timings_clear(void);
const struct ext4_io_stats *io_timings_get(uint32_t time_sum_ms);

#endif /* TEST_LWEXT4_H_ */
