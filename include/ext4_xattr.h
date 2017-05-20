/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2015 Kaho Ng (ngkaho1234@gmail.com)
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
 * @file  ext4_xattr.h
 * @brief Extended Attribute manipulation.
 */

#ifndef EXT4_XATTR_H_
#define EXT4_XATTR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_inode.h>

struct ext4_xattr_info {
	uint8_t name_index;
	const char *name;
	size_t name_len;
	const void *value;
	size_t value_len;
};

struct ext4_xattr_list_entry {
	uint8_t name_index;
	char *name;
	size_t name_len;
	struct ext4_xattr_list_entry *next;
};

struct ext4_xattr_search {
	/* The first entry in the buffer */
	struct ext4_xattr_entry *first;

	/* The address of the buffer */
	void *base;

	/* The first inaccessible address */
	void *end;

	/* The current entry pointer */
	struct ext4_xattr_entry *here;

	/* Entry not found */
	bool not_found;
};

const char *ext4_extract_xattr_name(const char *full_name, size_t full_name_len,
				    uint8_t *name_index, size_t *name_len,
				    bool *found);

const char *ext4_get_xattr_name_prefix(uint8_t name_index,
				       size_t *ret_prefix_len);

int ext4_xattr_list(struct ext4_inode_ref *inode_ref,
		    struct ext4_xattr_list_entry *list, size_t *list_len);

int ext4_xattr_get(struct ext4_inode_ref *inode_ref, uint8_t name_index,
		   const char *name, size_t name_len, void *buf, size_t buf_len,
		   size_t *data_len);

int ext4_xattr_remove(struct ext4_inode_ref *inode_ref, uint8_t name_index,
		      const char *name, size_t name_len);

int ext4_xattr_set(struct ext4_inode_ref *inode_ref, uint8_t name_index,
		   const char *name, size_t name_len, const void *value,
		   size_t value_len);

#ifdef __cplusplus
}
#endif

#endif
/**
 * @}
 */
