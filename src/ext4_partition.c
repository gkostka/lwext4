/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_partition.c
 * @brief Partition table parser: MBR (primary and logical partitions) and
 *        GPT (UEFI specification, chapter 5 "GUID Partition Table").
 */

#include <ext4_config.h>
#include <ext4_types.h>
#include <ext4_misc.h>
#include <ext4_errno.h>
#include <ext4_debug.h>
#include <ext4_crc32.h>

#include <ext4_partition.h>

#include <inttypes.h>
#include <string.h>

#define MBR_SIGNATURE 0xAA55
#define MBR_TYPE_GPT_PROTECTIVE 0xEE
#define MBR_STATUS_ACTIVE 0x80

#define GPT_SIGNATURE "EFI PART"
#define GPT_REVISION_MAJOR 1
#define GPT_HEADER_MIN_SIZE 92
#define GPT_ENTRY_MIN_SIZE 128
#define GPT_NAME_UNITS 36

#pragma pack(push, 1)

/*Partition record of an MBR or of an extended boot record (EBR)*/
struct part_mbr_entry {
	uint8_t status;
	uint8_t chs_first[3];
	uint8_t type;
	uint8_t chs_last[3];
	uint32_t first_lba;
	uint32_t sectors;
};

/*First 512 bytes of an MBR or EBR sector*/
struct part_mbr {
	uint8_t bootstrap[440];
	uint32_t disk_signature;
	uint16_t reserved;
	struct part_mbr_entry entry[4];
	uint16_t signature;
};

/*GPT header (UEFI specification, table "GPT Header")*/
struct part_gpt_header {
	uint8_t signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_crc32;
	uint32_t reserved;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	uint8_t disk_guid[16];
	uint64_t entries_lba;
	uint32_t num_entries;
	uint32_t entry_size;
	uint32_t entries_crc32;
};

/*GPT partition entry (UEFI specification, table "GPT Partition Entry")*/
struct part_gpt_entry {
	uint8_t type_guid[16];
	uint8_t unique_guid[16];
	uint64_t first_lba;
	uint64_t last_lba;
	uint64_t attributes;
	uint8_t name[GPT_NAME_UNITS * 2]; /*UTF-16LE*/
};

#pragma pack(pop)

/*Validated GPT header fields, host byte order*/
struct part_gpt_table {
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	uint64_t entries_lba;
	uint32_t num_entries;
	uint32_t entry_size;
	uint32_t entries_crc32;
};

static const uint8_t part_gpt_linux_fs[16] = EXT4_GPT_TYPE_LINUX_FS;

/**@brief Number of sectors (LBAs) of the parent device.*/
static uint64_t part_sectors(struct ext4_blockdev *parent)
{
	return parent->part_size / parent->bdif->ph_bsize;
}

/**@brief Read one whole sector into the interface's block buffer. Whole,
 *        aligned sectors are read straight into the buffer.*/
static int part_read_sector(struct ext4_blockdev *parent, uint64_t lba)
{
	uint32_t bsize = parent->bdif->ph_bsize;

	return ext4_block_readbytes(parent, lba * bsize, parent->bdif->ph_bbuf,
				    bsize);
}

/**@brief Forget the partitions found so far (keeps table and flags).*/
static void part_reset(struct ext4_part_bdevs *bdevs)
{
	memset(bdevs->info, 0, sizeof(bdevs->info));
	memset(bdevs->partitions, 0, sizeof(bdevs->partitions));
	bdevs->count = 0;
	bdevs->total = 0;
}

/**@brief Record a partition and set up its block device.
 * @return the partition's description, NULL if bdevs is full*/
static struct ext4_part_info *part_add(struct ext4_blockdev *parent,
				       struct ext4_part_bdevs *bdevs,
				       uint32_t number, uint64_t first_lba,
				       uint64_t last_lba)
{
	uint32_t bsize = parent->bdif->ph_bsize;
	struct ext4_part_info *info;
	struct ext4_blockdev *bd;

	ext4_dbg(DEBUG_MBR, DBG_INFO "partition %" PRIu32 ": lba %" PRIu64
		 " - %" PRIu64 "\n", number, first_lba, last_lba);

	bdevs->total++;
	if (bdevs->count >= CONFIG_EXT4_PARTITIONS_COUNT) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "partition %" PRIu32
			 ": no room, not reported\n", number);
		return NULL;
	}

	info = &bdevs->info[bdevs->count];
	bd = &bdevs->partitions[bdevs->count];
	bdevs->count++;

	info->number = number;
	info->first_lba = first_lba;
	info->last_lba = last_lba;

	bd->bdif = parent->bdif;
	bd->part_offset = parent->part_offset + first_lba * bsize;
	bd->part_size = (last_lba - first_lba + 1) * bsize;
	return info;
}

/******************************************************************************/
/* MBR                                                                        */
/******************************************************************************/

static bool mbr_is_extended(uint8_t type)
{
	return type == 0x05 || type == 0x0F || type == 0x85;
}

/**@brief Read the MBR and copy its four partition records.
 * @return EOK, ENOENT if the boot signature is missing, or a read error*/
static int mbr_read(struct ext4_blockdev *parent, uint64_t lba,
		    struct part_mbr_entry pe[4])
{
	const struct part_mbr *mbr = (const void *)parent->bdif->ph_bbuf;
	int r;

	r = part_read_sector(parent, lba);
	if (r != EOK)
		return r;

	if (to_le16(mbr->signature) != MBR_SIGNATURE) {
		ext4_dbg(DEBUG_MBR, DBG_INFO "lba %" PRIu64
			 ": no boot signature\n", lba);
		return ENOENT;
	}

	memcpy(pe, mbr->entry, sizeof(mbr->entry));
	return EOK;
}

/**@brief A protective MBR (UEFI specification, "Protective MBR") has a
 *        partition record of type 0xEE starting at LBA 1. Hybrid MBRs
 *        with further records are accepted too.*/
static bool mbr_is_protective(const struct part_mbr_entry pe[4])
{
	int i;

	for (i = 0; i < 4; i++)
		if (pe[i].type == MBR_TYPE_GPT_PROTECTIVE &&
		    to_le32(pe[i].first_lba) == 1)
			return true;
	return false;
}

static void mbr_set_info(struct ext4_part_info *info,
			 const struct part_mbr_entry *pe)
{
	if (!info)
		return;
	info->mbr_type = pe->type;
	info->attributes = pe->status & MBR_STATUS_ACTIVE;
}

/**@brief Follow the chain of extended boot records of the extended
 *        partition [ext_start, ext_start + ext_size).
 *
 * Every EBR holds a data partition record (start relative to the EBR) and
 * a link record of an extended type (start relative to ext_start) to the
 * next EBR. Records of both kinds are accepted in any of the four slots,
 * empty data records are skipped.*/
static int mbr_scan_logical(struct ext4_blockdev *parent,
			    struct ext4_part_bdevs *bdevs, uint64_t ext_start,
			    uint64_t ext_size, uint32_t *number)
{
	const uint64_t ext_end = ext_start + ext_size;
	struct part_mbr_entry pe[4];
	uint64_t ebr = ext_start;
	uint32_t steps;
	int r, i;

	for (steps = 0; steps < CONFIG_EXT4_MBR_EBR_MAX; steps++) {
		uint64_t next = 0;

		r = mbr_read(parent, ebr, pe);
		if (r == ENOENT) {
			ext4_dbg(DEBUG_MBR, DBG_ERROR "EBR at lba %" PRIu64
				 ": no signature\n", ebr);
			return EIO;
		}
		if (r != EOK)
			return r;

		for (i = 0; i < 4; i++) {
			uint64_t start = to_le32(pe[i].first_lba);
			uint64_t size = to_le32(pe[i].sectors);

			if (!pe[i].type || !size)
				continue; /*Empty record*/

			if (mbr_is_extended(pe[i].type)) {
				/*Link to the next EBR: first one wins*/
				if (!next)
					next = ext_start + start;
				continue;
			}

			start += ebr;
			if (start == ebr || start + size > ext_end) {
				ext4_dbg(DEBUG_MBR, DBG_WARN "EBR at lba %"
					 PRIu64 ": partition outside the "
					 "extended partition, skipped\n", ebr);
				continue;
			}

			mbr_set_info(part_add(parent, bdevs, (*number)++,
					      start, start + size - 1),
				     &pe[i]);
		}

		if (!next)
			return EOK;

		if (next >= ext_end) {
			ext4_dbg(DEBUG_MBR, DBG_ERROR "EBR at lba %" PRIu64
				 ": next EBR outside the extended partition\n",
				 ebr);
			return EIO;
		}
		ebr = next;
	}

	ext4_dbg(DEBUG_MBR, DBG_ERROR "more than %d EBRs: loop in the logical "
		 "partition chain?\n", (int)CONFIG_EXT4_MBR_EBR_MAX);
	return EIO;
}

/**@brief Report the primary partitions of pe (the MBR's records) and the
 *        logical partitions of its extended partitions.*/
static int mbr_scan(struct ext4_blockdev *parent,
		    struct ext4_part_bdevs *bdevs,
		    const struct part_mbr_entry pe[4])
{
	const uint64_t disk = part_sectors(parent);
	uint32_t number = 5;
	int r, i;

	bdevs->table = EXT4_PART_TABLE_MBR;

	for (i = 0; i < 4; i++) {
		uint64_t start = to_le32(pe[i].first_lba);
		uint64_t size = to_le32(pe[i].sectors);

		if (!pe[i].type || !size || mbr_is_extended(pe[i].type))
			continue;

		if (!start || start + size > disk) {
			ext4_dbg(DEBUG_MBR, DBG_WARN "partition %d outside "
				 "the device, skipped\n", i + 1);
			continue;
		}

		mbr_set_info(part_add(parent, bdevs, i + 1, start,
				      start + size - 1),
			     &pe[i]);
	}

	/*Logical partitions are numbered from 5, after the primaries*/
	for (i = 0; i < 4; i++) {
		uint64_t start = to_le32(pe[i].first_lba);
		uint64_t size = to_le32(pe[i].sectors);

		if (!size || !mbr_is_extended(pe[i].type))
			continue;

		if (!start || start + size > disk) {
			ext4_dbg(DEBUG_MBR, DBG_ERROR "extended partition %d "
				 "outside the device\n", i + 1);
			return EIO;
		}

		r = mbr_scan_logical(parent, bdevs, start, size, &number);
		if (r != EOK)
			return r;
	}

	return EOK;
}

/******************************************************************************/
/* GPT                                                                        */
/******************************************************************************/

/*CRC32 as used by the GPT: IEEE 802.3 polynomial, initial value and final
 * XOR 0xFFFFFFFF. ext4_crc32 is the table driven core without the pre- and
 * post-conditioning.*/
#define GPT_CRC32_INIT 0xFFFFFFFFul
#define GPT_CRC32_FINAL(crc) ((crc) ^ 0xFFFFFFFFul)

/**@brief Read and validate the GPT header at lba (UEFI specification,
 *        "GPT Header" validation rules) and the location of its partition
 *        entry array.
 * @return EOK and the header fields in t, EIO if invalid, or a read error*/
static int gpt_read_header(struct ext4_blockdev *parent, uint64_t lba,
			   struct part_gpt_table *t)
{
	const struct part_gpt_header *h = (const void *)parent->bdif->ph_bbuf;
	const uint8_t *raw = parent->bdif->ph_bbuf;
	const uint32_t bsize = parent->bdif->ph_bsize;
	const uint64_t disk = part_sectors(parent);
	const uint32_t zero = 0;
	uint64_t array_size, array_sectors;
	uint32_t header_size, crc;
	int r;

	r = part_read_sector(parent, lba);
	if (r != EOK)
		return r;

	if (memcmp(h->signature, GPT_SIGNATURE, sizeof(h->signature))) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": bad signature\n", lba);
		return EIO;
	}

	if ((to_le32(h->revision) >> 16) != GPT_REVISION_MAJOR) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": unsupported revision 0x%" PRIx32 "\n", lba,
			 to_le32(h->revision));
		return EIO;
	}

	header_size = to_le32(h->header_size);
	if (header_size < GPT_HEADER_MIN_SIZE || header_size > bsize) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": bad header size %" PRIu32 "\n", lba, header_size);
		return EIO;
	}

	/*The header CRC is computed with the CRC field itself zeroed*/
	crc = ext4_crc32(GPT_CRC32_INIT, raw, 16);
	crc = ext4_crc32(crc, &zero, sizeof(zero));
	crc = ext4_crc32(crc, raw + 20, header_size - 20);
	crc = GPT_CRC32_FINAL(crc);
	if (crc != to_le32(h->header_crc32)) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": header CRC mismatch\n", lba);
		return EIO;
	}

	if (to_le64(h->my_lba) != lba) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": MyLBA mismatch\n", lba);
		return EIO;
	}

	t->first_usable_lba = to_le64(h->first_usable_lba);
	t->last_usable_lba = to_le64(h->last_usable_lba);
	t->entries_lba = to_le64(h->entries_lba);
	t->num_entries = to_le32(h->num_entries);
	t->entry_size = to_le32(h->entry_size);
	t->entries_crc32 = to_le32(h->entries_crc32);

	/*Usable area: after the MBR and the primary header, before the
	 * backup header in the last LBA*/
	if (t->first_usable_lba < 2 ||
	    t->first_usable_lba > t->last_usable_lba ||
	    t->last_usable_lba >= disk - 1) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": bad usable LBA range\n", lba);
		return EIO;
	}

	/*Entry size: 128 * 2^n bytes*/
	if (t->entry_size < GPT_ENTRY_MIN_SIZE ||
	    (t->entry_size & (t->entry_size - 1))) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": bad entry size %" PRIu32 "\n", lba,
			 t->entry_size);
		return EIO;
	}

	/*Entry array: bounded in size, inside the device, overlapping
	 * neither the MBR, the header itself nor the usable area*/
	array_size = (uint64_t)t->num_entries * t->entry_size;
	array_sectors = (array_size + bsize - 1) / bsize;
	if (array_size > CONFIG_EXT4_GPT_ENTRIES_MAX_SIZE ||
	    t->entries_lba < 1 || t->entries_lba >= disk ||
	    array_sectors > disk - t->entries_lba ||
	    (lba >= t->entries_lba &&
	     lba < t->entries_lba + array_sectors) ||
	    (t->entries_lba + array_sectors > t->first_usable_lba &&
	     t->entries_lba <= t->last_usable_lba)) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT lba %" PRIu64
			 ": bad partition entry array location\n", lba);
		return EIO;
	}

	return EOK;
}

/**@brief Append the UTF-8 encoding of code point cp to out.*/
static char *gpt_put_utf8(char *out, uint32_t cp)
{
	if (cp < 0x80) {
		*out++ = (char)cp;
	} else if (cp < 0x800) {
		*out++ = (char)(0xC0 | (cp >> 6));
		*out++ = (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000ul) {
		*out++ = (char)(0xE0 | (cp >> 12));
		*out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
		*out++ = (char)(0x80 | (cp & 0x3F));
	} else {
		*out++ = (char)(0xF0 | (cp >> 18));
		*out++ = (char)(0x80 | ((cp >> 12) & 0x3F));
		*out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
		*out++ = (char)(0x80 | (cp & 0x3F));
	}
	return out;
}

/**@brief Convert a UTF-16LE GPT partition name to UTF-8. Unpaired
 *        surrogates become U+FFFD. Every code unit produces at most 3
 *        bytes, so out needs EXT4_PART_NAME_SIZE bytes.*/
static void gpt_name_to_utf8(char *out, const uint8_t *name)
{
	int i;

	for (i = 0; i < GPT_NAME_UNITS; i++) {
		uint32_t cp = name[2 * i] | ((uint32_t)name[2 * i + 1] << 8);

		if (!cp)
			break;

		if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < GPT_NAME_UNITS) {
			uint32_t lo = name[2 * i + 2] |
				      ((uint32_t)name[2 * i + 3] << 8);

			if (lo >= 0xDC00 && lo <= 0xDFFF) {
				cp = 0x10000ul + ((cp - 0xD800) << 10) +
				     (lo - 0xDC00);
				i++;
			}
		}

		if (cp >= 0xD800 && cp <= 0xDFFF)
			cp = 0xFFFD;

		out = gpt_put_utf8(out, cp);
	}
	*out = '\0';
}

static bool gpt_guid_is_zero(const uint8_t guid[16])
{
	int i;

	for (i = 0; i < 16; i++)
		if (guid[i])
			return false;
	return true;
}

/**@brief Report the partition described by entry index idx.*/
static void gpt_add_entry(struct ext4_blockdev *parent,
			  struct ext4_part_bdevs *bdevs,
			  const struct part_gpt_table *t, uint32_t idx,
			  const struct part_gpt_entry *e)
{
	struct ext4_part_info *info;
	uint64_t first, last;

	if (gpt_guid_is_zero(e->type_guid))
		return; /*Unused entry*/

	first = to_le64(e->first_lba);
	last = to_le64(e->last_lba);
	if (first > last || first < t->first_usable_lba ||
	    last > t->last_usable_lba) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT entry %" PRIu32
			 ": outside the usable LBA range, skipped\n", idx);
		return;
	}

	info = part_add(parent, bdevs, idx + 1, first, last);
	if (!info)
		return;

	memcpy(info->type_guid, e->type_guid, sizeof(info->type_guid));
	memcpy(info->unique_guid, e->unique_guid, sizeof(info->unique_guid));
	info->attributes = to_le64(e->attributes);
	gpt_name_to_utf8(info->name, e->name);
}

/**@brief Read the partition entry array described by t sector by sector,
 *        report its partitions and check its CRC.
 * @return EOK, EIO on CRC mismatch, or a read error*/
static int gpt_read_entries(struct ext4_blockdev *parent,
			    struct ext4_part_bdevs *bdevs,
			    const struct part_gpt_table *t)
{
	const uint8_t *buf = parent->bdif->ph_bbuf;
	const uint32_t bsize = parent->bdif->ph_bsize;
	const uint64_t array_size = (uint64_t)t->num_entries * t->entry_size;
	uint64_t lba = t->entries_lba;
	uint64_t off = 0;
	uint32_t crc = GPT_CRC32_INIT;
	uint32_t idx = 0;
	int r;

	while (off < array_size) {
		uint32_t len = bsize;

		if (array_size - off < len)
			len = (uint32_t)(array_size - off);

		r = part_read_sector(parent, lba++);
		if (r != EOK)
			return r;

		crc = ext4_crc32(crc, buf, len);

		/*Entries whose defined 128 bytes start in this sector. The
		 * sector size and the entry size are multiples of 128, so
		 * those bytes never cross a sector boundary.*/
		while (idx < t->num_entries &&
		       (uint64_t)idx * t->entry_size < off + len) {
			uint32_t pos =
			    (uint32_t)((uint64_t)idx * t->entry_size - off);

			gpt_add_entry(parent, bdevs, t, idx,
				      (const void *)(buf + pos));
			idx++;
		}

		off += len;
	}

	if (GPT_CRC32_FINAL(crc) != t->entries_crc32) {
		ext4_dbg(DEBUG_MBR, DBG_WARN "GPT: partition entry array CRC "
			 "mismatch\n");
		return EIO;
	}
	return EOK;
}

/**@brief Scan the GPT of a device with a protective MBR: the primary
 *        header in LBA 1, or if it or its entry array is invalid, the
 *        backup header in the last LBA.*/
static int gpt_scan(struct ext4_blockdev *parent,
		    struct ext4_part_bdevs *bdevs)
{
	const uint64_t disk = part_sectors(parent);
	struct part_gpt_table t;
	int r;

	bdevs->table = EXT4_PART_TABLE_GPT;
	if (disk < 3)
		return EIO;

	r = gpt_read_header(parent, 1, &t);
	if (r == EOK)
		r = gpt_read_entries(parent, bdevs, &t);
	if (r == EOK)
		return EOK;

	ext4_dbg(DEBUG_MBR, DBG_WARN "primary GPT invalid, trying the "
		 "backup\n");
	part_reset(bdevs);

	r = gpt_read_header(parent, disk - 1, &t);
	if (r == EOK)
		r = gpt_read_entries(parent, bdevs, &t);
	if (r != EOK) {
		ext4_dbg(DEBUG_MBR, DBG_ERROR "no valid GPT\n");
		return EIO;
	}

	bdevs->flags |= EXT4_PART_GPT_BACKUP_USED;
	return EOK;
}

/******************************************************************************/
/* Public interface                                                           */
/******************************************************************************/

enum part_mode { PART_AUTO, PART_MBR, PART_GPT };

static int part_scan(struct ext4_blockdev *parent,
		     struct ext4_part_bdevs *bdevs, enum part_mode mode)
{
	struct part_mbr_entry pe[4];
	uint32_t bsize;
	int r;

	memset(bdevs, 0, sizeof(struct ext4_part_bdevs));

	r = ext4_block_init(parent);
	if (r != EOK)
		return r;

	/*Records never cross a sector boundary for these sector sizes*/
	bsize = parent->bdif->ph_bsize;
	if (bsize < 512 || bsize % GPT_ENTRY_MIN_SIZE) {
		r = ENOTSUP;
		goto blockdev_fini;
	}

	r = mbr_read(parent, 0, pe);
	if (r != EOK)
		goto blockdev_fini;

	if (mode == PART_GPT && !mbr_is_protective(pe)) {
		ext4_dbg(DEBUG_MBR, DBG_INFO "no protective MBR\n");
		r = ENOENT;
		goto blockdev_fini;
	}

	if (mode == PART_GPT || (mode == PART_AUTO && mbr_is_protective(pe)))
		r = gpt_scan(parent, bdevs);
	else
		r = mbr_scan(parent, bdevs, pe);

blockdev_fini:
	ext4_block_fini(parent);
	if (r != EOK)
		memset(bdevs, 0, sizeof(struct ext4_part_bdevs));
	return r;
}

int ext4_partition_scan(struct ext4_blockdev *parent,
			struct ext4_part_bdevs *bdevs)
{
	ext4_dbg(DEBUG_MBR, DBG_INFO "ext4_partition_scan\n");
	return part_scan(parent, bdevs, PART_AUTO);
}

int ext4_mbr_scan_all(struct ext4_blockdev *parent,
		      struct ext4_part_bdevs *bdevs)
{
	ext4_dbg(DEBUG_MBR, DBG_INFO "ext4_mbr_scan_all\n");
	return part_scan(parent, bdevs, PART_MBR);
}

int ext4_gpt_scan(struct ext4_blockdev *parent, struct ext4_part_bdevs *bdevs)
{
	ext4_dbg(DEBUG_MBR, DBG_INFO "ext4_gpt_scan\n");
	return part_scan(parent, bdevs, PART_GPT);
}

bool ext4_part_is_linux(const struct ext4_part_info *info)
{
	if (info->mbr_type)
		return info->mbr_type == EXT4_MBR_TYPE_LINUX;
	return !memcmp(info->type_guid, part_gpt_linux_fs,
		       sizeof(part_gpt_linux_fs));
}

/**
 * @}
 */
