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
#ifndef FILE_DEV_H_
#define FILE_DEV_H_

#include <ext4_config.h>
#include <ext4_blockdev.h>

#include <stdint.h>
#include <stdbool.h>

/**@brief   File blockdev get.*/
struct ext4_blockdev *file_dev_get(void);

/**@brief   Set filename to open.*/
void file_dev_name_set(const char *n);

/**@brief   Set the byte offset of the partition within the file.
 *          Has to be a multiple of 512. Default: 0.
 *          Takes effect the next time the device is opened.
 * @param   offset partition offset in bytes*/
void file_dev_part_offset_set(uint64_t offset);

/**@brief   Set the byte size of the partition.
 *          Default: 0, meaning from the partition offset to the end of file.
 *          Opening the device fails with EINVAL if the partition does
 *          not fit in the file. Block reads and writes outside of the
 *          partition are rejected with EINVAL.
 *          Takes effect the next time the device is opened.
 * @param   size partition size in bytes*/
void file_dev_part_size_set(uint64_t size);

#endif /* FILE_DEV_H_ */
