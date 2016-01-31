/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
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

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_mbr.c
 * @brief Master boot record parser
 */

#include "ext4_config.h"
#include "ext4_types.h"
#include "ext4_misc.h"
#include "ext4_errno.h"
#include "ext4_debug.h"

#include "ext4_mbr.h"

#include <inttypes.h>
#include <string.h>

#define MBR_SIGNATURE 0xAA55

#pragma pack(push, 1)

struct ext4_part_entry {
	uint8_t status;
	uint8_t chs1[3];
	uint8_t type;
	uint8_t chs2[3];
	uint32_t first_lba;
	uint32_t sectors;
};

struct ext4_mbr {
	uint8_t bootstrap[446];
	struct ext4_part_entry part_entry[4];
	uint16_t signature;
};

#pragma pack(pop)

int ext4_mbr_scan(struct ext4_blockdev *parent, struct ext4_mbr_bdevs *bdevs)
{
	int r;
	size_t i;

	ext4_dbg(DEBUG_MBR, DBG_INFO "ext4_mbr_scan\n");
	memset(bdevs, 0, sizeof(struct ext4_mbr_bdevs));
	r = ext4_block_init(parent);
	if (r != EOK)
		return r;

	r = ext4_block_readbytes(parent, 0, parent->bdif->ph_bbuf, 512);
	if (r != EOK) {
		goto blockdev_fini;
	}

	const struct ext4_mbr *mbr = (void *)parent->bdif->ph_bbuf;

	if (to_le16(mbr->signature) != MBR_SIGNATURE) {
		ext4_dbg(DEBUG_MBR, DBG_ERROR "ext4_mbr_scan: unknown "
			 "signature: 0x%x\n", to_le16(mbr->signature));
		r = ENOENT;
		goto blockdev_fini;
	}

	/*Show bootstrap code*/
	ext4_dbg(DEBUG_MBR, "mbr_part: bootstrap:");
	for (i = 0; i < sizeof(mbr->bootstrap); ++i) {
		if (!(i & 0xF))
				ext4_dbg(DEBUG_MBR | DEBUG_NOPREFIX, "\n");
		ext4_dbg(DEBUG_MBR | DEBUG_NOPREFIX, "%02x, ", mbr->bootstrap[i]);
	}

	ext4_dbg(DEBUG_MBR | DEBUG_NOPREFIX, "\n\n");
	for (i = 0; i < 4; ++i) {
		const struct ext4_part_entry *pe = &mbr->part_entry[i];
		ext4_dbg(DEBUG_MBR, "mbr_part: %d\n", (int)i);
		ext4_dbg(DEBUG_MBR, "\tstatus: 0x%x\n", pe->status);
		ext4_dbg(DEBUG_MBR, "\ttype 0x%x:\n", pe->type);
		ext4_dbg(DEBUG_MBR, "\tfirst_lba: 0x%"PRIx32"\n", pe->first_lba);
		ext4_dbg(DEBUG_MBR, "\tsectors: 0x%"PRIx32"\n", pe->sectors);

		if (!pe->sectors)
			continue; /*Empty entry*/

		if (pe->type != 0x83)
			continue; /*Unsupported entry. 0x83 - linux native*/

		bdevs->partitions[i].bdif = parent->bdif;
		bdevs->partitions[i].part_offset =
			(uint64_t)pe->first_lba * parent->bdif->ph_bsize;
		bdevs->partitions[i].part_size =
			(uint64_t)pe->sectors * parent->bdif->ph_bsize;
	}

	blockdev_fini:
	ext4_block_fini(parent);
	return r;
}

/**
 * @}
 */

