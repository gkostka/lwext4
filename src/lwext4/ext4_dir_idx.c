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

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_dir_idx.c
 * @brief Directory indexing procedures.
 */

#include <ext4_config.h>
#include <ext4_dir_idx.h>
#include <ext4_dir.h>
#include <ext4_blockdev.h>
#include <ext4_fs.h>
#include <ext4_super.h>
#include <ext4_hash.h>

#include <string.h>
#include <stdlib.h>

struct ext4_dx_sort_entry {
    uint32_t hash;
    uint32_t rec_len;
    void 	 *dentry;
};

static int ext4_dir_dx_hash_string(struct ext4_hash_info *hinfo, int len,
    const char *name)
{
    return ext2_htree_hash(name, len, hinfo->seed, hinfo->hash_version,
            &hinfo->hash, &hinfo->minor_hash);
}


uint8_t ext4_dir_dx_root_info_get_hash_version(
        struct ext4_directory_dx_root_info *root_info)
{
    return root_info->hash_version;
}


void 	ext4_dir_dx_root_info_set_hash_version(
        struct ext4_directory_dx_root_info  *root_info, uint8_t v)
{
    root_info->hash_version = v;
}

uint8_t ext4_dir_dx_root_info_get_info_length(
        struct ext4_directory_dx_root_info *root_info)
{
    return root_info->info_length;
}
void 	ext4_dir_dx_root_info_set_info_length(
        struct ext4_directory_dx_root_info  *root_info, uint8_t len)
{
    root_info->info_length = len;
}

uint8_t ext4_dir_dx_root_info_get_indirect_levels(
        struct ext4_directory_dx_root_info *root_info)
{
    return root_info->indirect_levels;
}

void 	ext4_dir_dx_root_info_set_indirect_levels(
        struct ext4_directory_dx_root_info *root_info, uint8_t lvl)
{
    root_info->indirect_levels = lvl;
}



uint16_t ext4_dir_dx_countlimit_get_limit(
        struct ext4_directory_dx_countlimit *climit)
{
    return to_le16(climit->limit);
}
void 	ext4_dir_dx_countlimit_set_limit(
        struct ext4_directory_dx_countlimit *climit, uint16_t limit)
{
    climit->limit = to_le16(limit);
}

uint16_t ext4_dir_dx_countlimit_get_count(
        struct ext4_directory_dx_countlimit *climit)
{
    return to_le16(climit->count);
}

void 	ext4_dir_dx_countlimit_set_count(
        struct ext4_directory_dx_countlimit *climit, uint16_t count)
{
    climit->count = to_le16(count);
}


uint32_t ext4_dir_dx_entry_get_hash(
        struct ext4_directory_dx_entry *entry)
{
    return to_le32(entry->hash);
}
void ext4_dir_dx_entry_set_hash(
        struct ext4_directory_dx_entry *entry, uint32_t hash)
{
    entry->hash = to_le32(hash);
}

uint32_t ext4_dir_dx_entry_get_block(
        struct ext4_directory_dx_entry *entry)
{
    return to_le32(entry->block);
}
void 	ext4_dir_dx_entry_set_block(
        struct ext4_directory_dx_entry *entry, uint32_t block)
{
    entry->block = to_le32(block);
}
/****************************************************************************/

int 	ext4_dir_dx_init(struct ext4_inode_ref *dir)
{
    /* Load block 0, where will be index root located */
    uint32_t fblock;
    int rc = ext4_fs_get_inode_data_block_index(dir, 0,
            &fblock);
    if (rc != EOK)
        return rc;

    struct ext4_block block;
    rc = ext4_block_get(dir->fs->bdev, &block, fblock);
    if (rc != EOK)
        return rc;

    /* Initialize pointers to data structures */
    struct ext4_directory_dx_root *root = (void *)block.data;
    struct ext4_directory_dx_root_info *info = &(root->info);

    /* Initialize root info structure */
    uint8_t hash_version = ext4_get8(&dir->fs->sb, default_hash_version);


    ext4_dir_dx_root_info_set_hash_version(info, hash_version);
    ext4_dir_dx_root_info_set_indirect_levels(info, 0);
    ext4_dir_dx_root_info_set_info_length(info, 8);

    /* Set limit and current number of entries */
    struct ext4_directory_dx_countlimit *countlimit =
            (struct ext4_directory_dx_countlimit *) &root->entries;

    ext4_dir_dx_countlimit_set_count(countlimit, 1);

    uint32_t block_size =
            ext4_sb_get_block_size(&dir->fs->sb);
    uint32_t entry_space =
            block_size - 2 * sizeof(struct ext4_directory_dx_dot_entry) -
            sizeof(struct ext4_directory_dx_root_info);
    uint16_t root_limit = entry_space / sizeof(struct ext4_directory_dx_entry);
    ext4_dir_dx_countlimit_set_limit(countlimit, root_limit);

    /* Append new block, where will be new entries inserted in the future */
    uint32_t iblock;
    rc = ext4_fs_append_inode_block(dir, &fblock, &iblock);
    if (rc != EOK) {
        ext4_block_set(dir->fs->bdev, &block);
        return rc;
    }

    struct ext4_block new_block;

    rc = ext4_block_get(dir->fs->bdev, &new_block, fblock);
    if (rc != EOK) {
        ext4_block_set(dir->fs->bdev, &block);
        return rc;
    }

    /* Fill the whole block with empty entry */
    struct ext4_directory_entry_ll *block_entry = (void *)new_block.data;

    ext4_dir_entry_ll_set_entry_length(block_entry, block_size);
    ext4_dir_entry_ll_set_inode(block_entry, 0);

    new_block.dirty = true;
    rc = ext4_block_set(dir->fs->bdev, &new_block);
    if (rc != EOK) {
        ext4_block_set(dir->fs->bdev, &block);
        return rc;
    }

    /* Connect new block to the only entry in index */
    struct ext4_directory_dx_entry *entry = root->entries;
    ext4_dir_dx_entry_set_block(entry, iblock);

    block.dirty = true;

    return ext4_block_set(dir->fs->bdev, &block);
}

static int ext4_dir_hinfo_init(struct ext4_hash_info *hinfo,
        struct ext4_block *root_block, struct ext4_sblock *sb, size_t name_len,
        const char *name)
{
    struct ext4_directory_dx_root *root =
            (struct ext4_directory_dx_root *) root_block->data;

    if ((root->info.hash_version != EXT2_HTREE_LEGACY) &&
            (root->info.hash_version != EXT2_HTREE_HALF_MD4) &&
            (root->info.hash_version != EXT2_HTREE_TEA))
        return EXT4_ERR_BAD_DX_DIR;

    /* Check unused flags */
    if (root->info.unused_flags != 0)
        return EXT4_ERR_BAD_DX_DIR;

    /* Check indirect levels */
    if (root->info.indirect_levels > 1)
        return EXT4_ERR_BAD_DX_DIR;

    /* Check if node limit is correct */
    uint32_t block_size = ext4_sb_get_block_size(sb);
    uint32_t entry_space = block_size;
    entry_space -= 2 * sizeof(struct ext4_directory_dx_dot_entry);
    entry_space -= sizeof(struct ext4_directory_dx_root_info);
    entry_space = entry_space / sizeof(struct ext4_directory_dx_entry);

    uint16_t limit = ext4_dir_dx_countlimit_get_limit(
            (struct ext4_directory_dx_countlimit *) &root->entries);
    if (limit != entry_space)
        return EXT4_ERR_BAD_DX_DIR;

    /* Check hash version and modify if necessary */
    hinfo->hash_version =
            ext4_dir_dx_root_info_get_hash_version(&root->info);
    if ((hinfo->hash_version <= EXT2_HTREE_TEA) &&
            (ext4_sb_check_flag(sb, EXT4_SUPERBLOCK_FLAGS_UNSIGNED_HASH))) {
        /* Use unsigned hash */
        hinfo->hash_version += 3;
    }

    /* Load hash seed from superblock */

    hinfo->seed = ext4_get8(sb, hash_seed);

    /* Compute hash value of name */
    if (name)
        return ext4_dir_dx_hash_string(hinfo, name_len, name);

    return EOK;
}


static int ext4_dir_dx_get_leaf(struct ext4_hash_info *hinfo,
        struct ext4_inode_ref *inode_ref, struct ext4_block *root_block,
        struct ext4_directory_dx_block **dx_block,
        struct ext4_directory_dx_block *dx_blocks)
{
    struct ext4_directory_dx_block *tmp_dx_block = dx_blocks;
    struct ext4_directory_dx_root *root =
            (struct ext4_directory_dx_root *) root_block->data;
    struct ext4_directory_dx_entry *entries =
            (struct ext4_directory_dx_entry *) &root->entries;

    uint16_t limit = ext4_dir_dx_countlimit_get_limit(
            (struct ext4_directory_dx_countlimit *) entries);
    uint8_t indirect_level =
            ext4_dir_dx_root_info_get_indirect_levels(&root->info);

    struct ext4_block *tmp_block = root_block;
    struct ext4_directory_dx_entry *p;
    struct ext4_directory_dx_entry *q;
    struct ext4_directory_dx_entry *m;
    struct ext4_directory_dx_entry *at;

    /* Walk through the index tree */
    while (true) {
        uint16_t count = ext4_dir_dx_countlimit_get_count(
                (struct ext4_directory_dx_countlimit *) entries);
        if ((count == 0) || (count > limit))
            return EXT4_ERR_BAD_DX_DIR;

        /* Do binary search in every node */
        p = entries + 1;
        q = entries + count - 1;

        while (p <= q) {
            m = p + (q - p) / 2;
            if (ext4_dir_dx_entry_get_hash(m) > hinfo->hash)
                q = m - 1;
            else
                p = m + 1;
        }

        at = p - 1;

        /* Write results */

        memcpy(&tmp_dx_block->block, tmp_block, sizeof(struct ext4_block));
        tmp_dx_block->entries = entries;
        tmp_dx_block->position = at;

        /* Is algorithm in the leaf? */
        if (indirect_level == 0) {
            *dx_block = tmp_dx_block;
            return EOK;
        }

        /* Goto child node */
        uint32_t next_block = ext4_dir_dx_entry_get_block(at);

        indirect_level--;

        uint32_t fblock;
        int rc = ext4_fs_get_inode_data_block_index(inode_ref,
                next_block, &fblock);
        if (rc != EOK)
            return rc;

        rc = ext4_block_get(inode_ref->fs->bdev, tmp_block, fblock);
        if (rc != EOK)
            return rc;

        entries = ((struct ext4_directory_dx_node *) tmp_block->data)->entries;
        limit = ext4_dir_dx_countlimit_get_limit(
                (struct ext4_directory_dx_countlimit *) entries);

        uint16_t entry_space =
                ext4_sb_get_block_size(&inode_ref->fs->sb) -
                sizeof(struct ext4_directory_dx_dot_entry);

        entry_space = entry_space / sizeof(struct ext4_directory_dx_entry);

        if (limit != entry_space) {
            ext4_block_set(inode_ref->fs->bdev, tmp_block);
            return EXT4_ERR_BAD_DX_DIR;
        }

        ++tmp_dx_block;
    }

    /* Unreachable */
    return EOK;
}

static int ext4_dir_dx_next_block(struct ext4_inode_ref *inode_ref,
        uint32_t hash, struct ext4_directory_dx_block *dx_block,
        struct ext4_directory_dx_block *dx_blocks)
{
    uint32_t num_handles = 0;
    struct ext4_directory_dx_block *p = dx_block;

    /* Try to find data block with next bunch of entries */
    while (true) {
        p->position++;
        uint16_t count = ext4_dir_dx_countlimit_get_count(
                (struct ext4_directory_dx_countlimit *) p->entries);

        if (p->position < p->entries + count)
            break;

        if (p == dx_blocks)
            return EOK;

        num_handles++;
        p--;
    }

    /* Check hash collision (if not occured - no next block cannot be used)*/
    uint32_t current_hash = ext4_dir_dx_entry_get_hash(p->position);
    if ((hash & 1) == 0) {
        if ((current_hash & ~1) != hash)
            return 0;
    }

    /* Fill new path */
    while (num_handles--) {
        uint32_t block_idx =
                ext4_dir_dx_entry_get_block(p->position);
        uint32_t block_addr;

        int rc = ext4_fs_get_inode_data_block_index(inode_ref,
                block_idx, &block_addr);
        if (rc != EOK)
            return rc;


        struct ext4_block block;
        rc = ext4_block_get(inode_ref->fs->bdev, &block, block_addr);
        if (rc != EOK)
            return rc;

        p++;

        /* Don't forget to put old block (prevent memory leak) */
        ext4_block_set(inode_ref->fs->bdev, &p->block);



        memcpy(&p->block, &p->block, sizeof(block));
        p->entries = ((struct ext4_directory_dx_node *) block.data)->entries;
        p->position = p->entries;
    }

    return ENOENT;
}



int 	ext4_dir_dx_find_entry(struct ext4_directory_search_result * result,
        struct ext4_inode_ref *inode_ref, size_t name_len, const char *name)
{
    /* Load direct block 0 (index root) */
    uint32_t root_block_addr;
    int rc = ext4_fs_get_inode_data_block_index(inode_ref, 0,
            &root_block_addr);
    if (rc != EOK)
        return rc;

    struct ext4_fs *fs = inode_ref->fs;

    struct ext4_block root_block;
    rc = ext4_block_get(fs->bdev, &root_block, root_block_addr);
    if (rc != EOK)
        return rc;

    /* Initialize hash info (compute hash value) */
    struct ext4_hash_info hinfo;
    rc = ext4_dir_hinfo_init(&hinfo, &root_block, &fs->sb,
            name_len, name);
    if (rc != EOK) {
        ext4_block_set(fs->bdev, &root_block);
        return EXT4_ERR_BAD_DX_DIR;
    }

    /*
     * Hardcoded number 2 means maximum height of index tree,
     * specified in the Linux driver.
     */
    struct ext4_directory_dx_block dx_blocks[2];
    struct ext4_directory_dx_block *dx_block;
    struct ext4_directory_dx_block *tmp;

    rc = ext4_dir_dx_get_leaf(&hinfo, inode_ref, &root_block,
            &dx_block, dx_blocks);
    if (rc != EOK) {
        ext4_block_set(fs->bdev, &root_block);
        return EXT4_ERR_BAD_DX_DIR;
    }

    do {
        /* Load leaf block */
        uint32_t leaf_block_idx =
                ext4_dir_dx_entry_get_block(dx_block->position);
        uint32_t leaf_block_addr;

        rc = ext4_fs_get_inode_data_block_index(inode_ref,
                leaf_block_idx, &leaf_block_addr);
        if (rc != EOK)
            goto cleanup;

        struct ext4_block leaf_block;
        rc = ext4_block_get(fs->bdev, &leaf_block, leaf_block_addr);
        if (rc != EOK)
            goto cleanup;

        /* Linear search inside block */
        struct ext4_directory_entry_ll *res_dentry;
        rc = ext4_dir_find_in_block(&leaf_block, &fs->sb,
                name_len, name, &res_dentry);

        /* Found => return it */
        if (rc == EOK) {
            result->block = leaf_block;
            result->dentry = res_dentry;
            goto cleanup;
        }

        /* Not found, leave untouched */
        ext4_block_set(fs->bdev, &leaf_block);

        if (rc != ENOENT)
            goto cleanup;

        /* check if the next block could be checked */
        rc = ext4_dir_dx_next_block(inode_ref, hinfo.hash,
                dx_block, &dx_blocks[0]);
        if (rc < 0)
            goto cleanup;
    } while (rc == ENOENT);

    /* Entry not found */
    rc = ENOENT;

    cleanup:
    /* The whole path must be released (preventing memory leak) */
    tmp = dx_blocks;

    while (tmp <= dx_block) {
        ext4_block_set(fs->bdev, &tmp->block);
        ++tmp;
    }

    return rc;
}

static int ext4_dir_dx_entry_comparator(const void *arg1, const void *arg2)
{
    struct ext4_dx_sort_entry *entry1 = (void *)arg1;
    struct ext4_dx_sort_entry *entry2 = (void *)arg2;

    if (entry1->hash == entry2->hash)
        return 0;

    if (entry1->hash < entry2->hash)
        return -1;
    else
        return 1;
}

static void ext4_dir_dx_insert_entry(
        struct ext4_directory_dx_block *index_block, uint32_t hash,
        uint32_t iblock)
{
    struct ext4_directory_dx_entry *old_index_entry = index_block->position;
    struct ext4_directory_dx_entry *new_index_entry = old_index_entry + 1;

    struct ext4_directory_dx_countlimit *countlimit =
            (struct ext4_directory_dx_countlimit *) index_block->entries;
    uint32_t count = ext4_dir_dx_countlimit_get_count(countlimit);

    struct ext4_directory_dx_entry *start_index = index_block->entries;
    size_t bytes = (void *) (start_index + count) - (void *) (new_index_entry);

    memmove(new_index_entry + 1, new_index_entry, bytes);

    ext4_dir_dx_entry_set_block(new_index_entry, iblock);
    ext4_dir_dx_entry_set_hash(new_index_entry, hash);

    ext4_dir_dx_countlimit_set_count(countlimit, count + 1);

    index_block->block.dirty = true;
}

static int ext4_dir_dx_split_data(struct ext4_inode_ref *inode_ref,
        struct ext4_hash_info *hinfo, struct ext4_block *old_data_block,
        struct ext4_directory_dx_block *index_block,
        struct ext4_block *new_data_block)
{
    int rc = EOK;

    /* Allocate buffer for directory entries */
    uint32_t block_size =
            ext4_sb_get_block_size(&inode_ref->fs->sb);

    void *entry_buffer = malloc(block_size);
    if (entry_buffer == NULL)
        return ENOMEM;

    /* dot entry has the smallest size available */
    uint32_t max_entry_count =
            block_size / sizeof(struct ext4_directory_dx_dot_entry);

    /* Allocate sort entry */
    struct ext4_dx_sort_entry *sort_array =
            malloc(max_entry_count * sizeof(struct ext4_dx_sort_entry));

    if (sort_array == NULL) {
        free(entry_buffer);
        return ENOMEM;
    }

    uint32_t idx = 0;
    uint32_t real_size = 0;

    /* Initialize hinfo */
    struct ext4_hash_info tmp_hinfo;
    memcpy(&tmp_hinfo, hinfo, sizeof(struct ext4_hash_info));

    /* Load all valid entries to the buffer */
    struct ext4_directory_entry_ll *dentry = (void *)old_data_block->data;
    void *entry_buffer_ptr = entry_buffer;
    while ((void *)dentry < (void *)(old_data_block->data + block_size)) {
        /* Read only valid entries */
        if (ext4_dir_entry_ll_get_inode(dentry) != 0) {
            uint8_t len = ext4_dir_entry_ll_get_name_length(
                    &inode_ref->fs->sb, dentry);

            rc = ext4_dir_dx_hash_string(&tmp_hinfo, len, (char*)dentry->name);
            if(rc != EOK) {
                free(sort_array);
                free(entry_buffer);
                return rc;
            }

            uint32_t rec_len = 8 + len;

            if ((rec_len % 4) != 0)
                rec_len += 4 - (rec_len % 4);

            memcpy(entry_buffer_ptr, dentry, rec_len);

            sort_array[idx].dentry = entry_buffer_ptr;
            sort_array[idx].rec_len = rec_len;
            sort_array[idx].hash = tmp_hinfo.hash;

            entry_buffer_ptr += rec_len;
            real_size += rec_len;
            idx++;
        }

        dentry = (void *) dentry +
                ext4_dir_entry_ll_get_entry_length(dentry);
    }

    /* Sort all entries */
    qsort(sort_array, idx, sizeof(struct ext4_dx_sort_entry),
            ext4_dir_dx_entry_comparator);

    /* Allocate new block for store the second part of entries */
    uint32_t new_fblock;
    uint32_t new_iblock;
    rc = ext4_fs_append_inode_block(inode_ref, &new_fblock,
            &new_iblock);
    if (rc != EOK) {
        free(sort_array);
        free(entry_buffer);
        return rc;
    }

    /* Load new block */
    struct ext4_block new_data_block_tmp;
    rc = ext4_block_get(inode_ref->fs->bdev, &new_data_block_tmp,
            new_fblock);
    if (rc != EOK) {
        free(sort_array);
        free(entry_buffer);
        return rc;
    }

    /*
     * Distribute entries to two blocks (by size)
     * - compute the half
     */
    uint32_t new_hash = 0;
    uint32_t current_size = 0;
    uint32_t mid = 0;
    uint32_t i;
    for ( i = 0; i < idx; ++i) {
        if ((current_size + sort_array[i].rec_len) > (real_size / 2)) {
            new_hash = sort_array[i].hash;
            mid = i;
            break;
        }

        current_size += sort_array[i].rec_len;
    }

    /* Check hash collision */
    uint32_t continued = 0;
    if (new_hash == sort_array[mid-1].hash)
        continued = 1;

    uint32_t offset = 0;
    void *ptr;

    /* First part - to the old block */
    for (i = 0; i < mid; ++i) {
        ptr = old_data_block->data + offset;
        memcpy(ptr, sort_array[i].dentry, sort_array[i].rec_len);

        struct ext4_directory_entry_ll *tmp = ptr;
        if (i < (mid - 1))
            ext4_dir_entry_ll_set_entry_length(tmp,
                    sort_array[i].rec_len);
        else
            ext4_dir_entry_ll_set_entry_length(tmp,
                    block_size - offset);

        offset += sort_array[i].rec_len;
    }

    /* Second part - to the new block */
    offset = 0;
    for (i = mid; i < idx; ++i) {
        ptr = new_data_block_tmp.data + offset;
        memcpy(ptr, sort_array[i].dentry, sort_array[i].rec_len);

        struct ext4_directory_entry_ll *tmp = ptr;
        if (i < (idx - 1))
            ext4_dir_entry_ll_set_entry_length(tmp,
                    sort_array[i].rec_len);
        else
            ext4_dir_entry_ll_set_entry_length(tmp,
                    block_size - offset);

        offset += sort_array[i].rec_len;
    }

    /* Do some steps to finish operation */
    old_data_block->dirty = true;
    new_data_block_tmp.dirty = true;

    free(sort_array);
    free(entry_buffer);

    ext4_dir_dx_insert_entry(index_block, new_hash + continued,
            new_iblock);

    *new_data_block = new_data_block_tmp;

    return EOK;
}

/** Split index node and maybe some parent nodes in the tree hierarchy.
 *
 * @param inode_ref Directory i-node
 * @param dx_blocks Array with path from root to leaf node
 * @param dx_block  Leaf block to be split if needed
 *
 * @return Error code
 *
 */
static int ext4_dir_dx_split_index(struct ext4_inode_ref *inode_ref,
        struct  ext4_directory_dx_block *dx_blocks,
        struct ext4_directory_dx_block *dx_block)
{
    struct ext4_directory_dx_entry *entries;
    if (dx_block == dx_blocks)
        entries =
                ((struct  ext4_directory_dx_root *) dx_block->block.data)->entries;
    else
        entries =
                ((struct ext4_directory_dx_node *) dx_block->block.data)->entries;

    struct ext4_directory_dx_countlimit *countlimit =
            (struct ext4_directory_dx_countlimit *) entries;

    uint16_t leaf_limit =
            ext4_dir_dx_countlimit_get_limit(countlimit);
    uint16_t leaf_count =
            ext4_dir_dx_countlimit_get_count(countlimit);

    /* Check if is necessary to split index block */
    if (leaf_limit == leaf_count) {
        size_t levels = dx_block - dx_blocks;

        struct ext4_directory_dx_entry *root_entries =
                ((struct ext4_directory_dx_root *) dx_blocks[0].block.data)->entries;

        struct ext4_directory_dx_countlimit *root_countlimit =
                (struct ext4_directory_dx_countlimit *) root_entries;
        uint16_t root_limit =
                ext4_dir_dx_countlimit_get_limit(root_countlimit);
        uint16_t root_count =
                ext4_dir_dx_countlimit_get_count(root_countlimit);

        /* Linux limitation */
        if ((levels > 0) && (root_limit == root_count))
            return ENOSPC;

        /* Add new block to directory */
        uint32_t new_fblock;
        uint32_t new_iblock;
        int rc = ext4_fs_append_inode_block(inode_ref,
                &new_fblock, &new_iblock);
        if (rc != EOK)
            return rc;

        /* load new block */
        struct ext4_block new_block;
        rc = ext4_block_get(inode_ref->fs->bdev, &new_block,
                new_fblock);
        if (rc != EOK)
            return rc;

        struct ext4_directory_dx_node  *new_node 	= (void *)new_block.data;
        struct ext4_directory_dx_entry *new_entries = new_node->entries;

        uint32_t block_size =
                ext4_sb_get_block_size(&inode_ref->fs->sb);

        /* Split leaf node */
        if (levels > 0) {
            uint32_t count_left = leaf_count / 2;
            uint32_t count_right = leaf_count - count_left;
            uint32_t hash_right =
                    ext4_dir_dx_entry_get_hash(entries + count_left);

            /* Copy data to new node */
            memcpy((void *) new_entries, (void *) (entries + count_left),
                    count_right * sizeof(struct ext4_directory_dx_entry));

            /* Initialize new node */
            struct ext4_directory_dx_countlimit *left_countlimit =
                    (struct ext4_directory_dx_countlimit *) entries;
            struct ext4_directory_dx_countlimit *right_countlimit =
                    (struct ext4_directory_dx_countlimit *) new_entries;

            ext4_dir_dx_countlimit_set_count(left_countlimit, count_left);
            ext4_dir_dx_countlimit_set_count(right_countlimit, count_right);

            uint32_t entry_space =
                    block_size - sizeof(struct ext4_fake_directory_entry);
            uint32_t node_limit =
                    entry_space / sizeof(struct ext4_directory_dx_entry);
            ext4_dir_dx_countlimit_set_limit(right_countlimit, node_limit);

            /* Which index block is target for new entry */
            uint32_t position_index = (dx_block->position - dx_block->entries);
            if (position_index >= count_left) {
                dx_block->block.dirty = true;

                struct ext4_block block_tmp = dx_block->block;


                dx_block->block = new_block;

                dx_block->position =
                        new_entries + position_index - count_left;
                dx_block->entries = new_entries;

                new_block = block_tmp;
            }

            /* Finally insert new entry */
            ext4_dir_dx_insert_entry(dx_blocks, hash_right, new_iblock);

            return ext4_block_set(inode_ref->fs->bdev, &new_block);
        } else {
            /* Create second level index */

            /* Copy data from root to child block */
            memcpy((void *) new_entries, (void *) entries,
                    leaf_count * sizeof(struct ext4_directory_dx_entry));

            struct ext4_directory_dx_countlimit *new_countlimit =
                    (struct ext4_directory_dx_countlimit *) new_entries;

            uint32_t entry_space =
                    block_size - sizeof(struct ext4_fake_directory_entry);
            uint32_t node_limit =
                    entry_space / sizeof(struct ext4_directory_dx_entry);
            ext4_dir_dx_countlimit_set_limit(new_countlimit, node_limit);

            /* Set values in root node */
            struct ext4_directory_dx_countlimit *new_root_countlimit =
                    (struct ext4_directory_dx_countlimit *) entries;

            ext4_dir_dx_countlimit_set_count(new_root_countlimit, 1);
            ext4_dir_dx_entry_set_block(entries, new_iblock);

            ((struct ext4_directory_dx_root *)
                    dx_blocks[0].block.data)->info.indirect_levels = 1;

            /* Add new entry to the path */
            dx_block = dx_blocks + 1;
            dx_block->position = dx_block->position - entries + new_entries;
            dx_block->entries = new_entries;
            dx_block->block = new_block;
        }
    }

    return EOK;
}

int 	ext4_dir_dx_add_entry(struct ext4_inode_ref *parent,
        struct ext4_inode_ref *child, const char *name)
{
    int rc2 = EOK;

    /* Get direct block 0 (index root) */
    uint32_t root_block_addr;
    int rc = ext4_fs_get_inode_data_block_index(parent, 0,
            &root_block_addr);
    if (rc != EOK)
        return rc;

    struct ext4_fs *fs = parent->fs;
    struct ext4_block root_block;

    rc = ext4_block_get(fs->bdev, &root_block, root_block_addr);
    if (rc != EOK)
        return rc;

    /* Initialize hinfo structure (mainly compute hash) */
    uint32_t name_len = strlen(name);
    struct ext4_hash_info hinfo;
    rc = ext4_dir_hinfo_init(&hinfo, &root_block, &fs->sb,
            name_len, name);
    if (rc != EOK) {
        ext4_block_set(fs->bdev, &root_block);
        return EXT4_ERR_BAD_DX_DIR;
    }

    /*
     * Hardcoded number 2 means maximum height of index
     * tree defined in Linux.
     */
    struct ext4_directory_dx_block dx_blocks[2];
    struct ext4_directory_dx_block *dx_block;
    struct ext4_directory_dx_block *dx_it;

    rc = ext4_dir_dx_get_leaf(&hinfo, parent, &root_block,
            &dx_block, dx_blocks);
    if (rc != EOK) {
        rc = EXT4_ERR_BAD_DX_DIR;
        goto release_index;
    }

    /* Try to insert to existing data block */
    uint32_t leaf_block_idx =
            ext4_dir_dx_entry_get_block(dx_block->position);
    uint32_t leaf_block_addr;
    rc = ext4_fs_get_inode_data_block_index(parent, leaf_block_idx,
            &leaf_block_addr);
    if (rc != EOK)
        goto release_index;

    struct ext4_block target_block;
    rc = ext4_block_get(fs->bdev, &target_block, leaf_block_addr);
    if (rc != EOK)
        goto release_index;

    /* Check if insert operation passed */
    rc = ext4_dir_try_insert_entry(&fs->sb, &target_block, child,
            name, name_len);
    if (rc == EOK)
        goto release_target_index;

    /*
     * Check if there is needed to split index node
     * (and recursively also parent nodes)
     */
    rc = ext4_dir_dx_split_index(parent, dx_blocks, dx_block);
    if (rc != EOK)
        goto release_target_index;

    /* Split entries to two blocks (includes sorting by hash value) */
    struct ext4_block new_block;
    rc = ext4_dir_dx_split_data(parent, &hinfo, &target_block,
            dx_block, &new_block);
    if (rc != EOK) {
        rc2 = rc;
        goto release_target_index;
    }

    /* Where to save new entry */
    uint32_t new_block_hash =
            ext4_dir_dx_entry_get_hash(dx_block->position + 1);
    if (hinfo.hash >= new_block_hash)
        rc = ext4_dir_try_insert_entry(&fs->sb, &new_block,
                child, name, name_len);
    else
        rc = ext4_dir_try_insert_entry(&fs->sb, &target_block,
                child, name, name_len);

    /* Cleanup */
    rc = ext4_block_set(fs->bdev, &new_block);
    if (rc != EOK)
        return rc;

    /* Cleanup operations */

    release_target_index:
    rc2 = rc;

    rc = ext4_block_set(fs->bdev, &target_block);
    if (rc != EOK)
        return rc;

    release_index:
    if (rc != EOK)
        rc2 = rc;

    dx_it = dx_blocks;

    while (dx_it <= dx_block) {
        rc = ext4_block_set(fs->bdev, &dx_it->block);
        if (rc != EOK)
            return rc;

        dx_it++;
    }

    return rc2;
}

/**
 * @}
 */

