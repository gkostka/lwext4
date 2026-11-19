/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_partition.h
 * @brief Partition table parser: MBR (primary and logical partitions) and
 *        GPT (GUID partition table, UEFI specification chapter 5).
 *
 * ext4_partition_scan() detects the partition table of a block device and
 * creates one block device per partition, like ext4_mbr_scan() does for the
 * four primary MBR partitions (ext4_mbr_scan() and struct ext4_mbr_bdevs are
 * unchanged). The partition block devices share the parent's block device
 * interface, so they can be registered with ext4_device_register() and
 * mounted one after the other.
 */

#ifndef EXT4_PARTITION_H_
#define EXT4_PARTITION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>
#include <ext4_blockdev.h>

#include <stdbool.h>
#include <stdint.h>

/**@brief Maximum number of partitions a scan reports. Partitions beyond
 *        this count are counted in ext4_part_bdevs::total but not stored.*/
#ifndef CONFIG_EXT4_PARTITIONS_COUNT
#define CONFIG_EXT4_PARTITIONS_COUNT 16
#endif

/**@brief Maximum number of extended boot records followed in the logical
 *        partition chain of an MBR. A longer chain (or a loop) is an error.*/
#ifndef CONFIG_EXT4_MBR_EBR_MAX
#define CONFIG_EXT4_MBR_EBR_MAX 128
#endif

/**@brief Maximum size in bytes of a GPT partition entry array (the UEFI
 *        minimum is 16 KiB: 128 entries of 128 bytes).*/
#ifndef CONFIG_EXT4_GPT_ENTRIES_MAX_SIZE
#define CONFIG_EXT4_GPT_ENTRIES_MAX_SIZE (1024ul * 1024ul)
#endif

/**@brief Size of ext4_part_info::name: the 36 UTF-16 code units of a GPT
 *        partition name need at most 3 bytes each in UTF-8, plus the NUL.*/
#define EXT4_PART_NAME_SIZE (36 * 3 + 1)

/**@brief GPT partition type GUID "Linux filesystem data"
 *        (0FC63DAF-8483-4772-8E79-3D69D8477DE4) in on-disk byte order.*/
#define EXT4_GPT_TYPE_LINUX_FS                                                 \
	{                                                                      \
		0xaf, 0x3d, 0xc6, 0x0f, 0x83, 0x84, 0x72, 0x47,                \
		0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4                 \
	}

/**@brief MBR partition type of Linux native partitions.*/
#define EXT4_MBR_TYPE_LINUX 0x83

/**@brief Partition table types.*/
enum ext4_part_table {
	EXT4_PART_TABLE_NONE = 0,
	EXT4_PART_TABLE_MBR = 1,
	EXT4_PART_TABLE_GPT = 2,
};

/**@brief ext4_part_bdevs::flags: the primary GPT header or its partition
 *        entry array is invalid, the partitions come from the backup GPT
 *        at the end of the device. Tools may want to repair the primary.*/
#define EXT4_PART_GPT_BACKUP_USED 0x1

/**@brief Description of one partition.*/
struct ext4_part_info {
	/**@brief Partition number as Linux counts them: MBR primary
	 *        partitions are 1-4 (by slot), logical partitions 5, 6, ...
	 *        in chain order; GPT partitions are entry index + 1.*/
	uint32_t number;

	/**@brief MBR partition type (system id), 0 for GPT.*/
	uint8_t mbr_type;

	/**@brief GPT partition type GUID in on-disk byte order (all zero
	 *        for MBR).*/
	uint8_t type_guid[16];

	/**@brief GPT unique partition GUID in on-disk byte order (all zero
	 *        for MBR).*/
	uint8_t unique_guid[16];

	/**@brief GPT attribute bits (MBR: 0x80 if the partition is marked
	 *        active/bootable, 0 otherwise).*/
	uint64_t attributes;

	/**@brief First sector (LBA) of the partition.*/
	uint64_t first_lba;

	/**@brief Last sector (LBA) of the partition, inclusive.*/
	uint64_t last_lba;

	/**@brief GPT partition name converted to UTF-8, NUL terminated
	 *        (empty for MBR).*/
	char name[EXT4_PART_NAME_SIZE];
};

/**@brief Partition block devices descriptor.*/
struct ext4_part_bdevs {
	/**@brief Partition table found on the device.*/
	enum ext4_part_table table;

	/**@brief EXT4_PART_* flags.*/
	uint32_t flags;

	/**@brief Number of partitions stored in info/partitions.*/
	uint32_t count;

	/**@brief Number of partitions in the table. Larger than count if
	 *        the table has more than CONFIG_EXT4_PARTITIONS_COUNT.*/
	uint32_t total;

	/**@brief Description of partition i.*/
	struct ext4_part_info info[CONFIG_EXT4_PARTITIONS_COUNT];

	/**@brief Block device of partition i.*/
	struct ext4_blockdev partitions[CONFIG_EXT4_PARTITIONS_COUNT];
};

/**@brief Scan the partition table of a block device.
 *
 * A GPT is used if the MBR is a protective MBR (a partition of type 0xEE
 * starting at LBA 1), otherwise the MBR partitions, including the logical
 * partitions of extended partitions (types 0x05, 0x0F, 0x85), are reported.
 * All non-empty partitions are reported; extended partitions themselves are
 * not. Partitions that do not lie inside the device (GPT: inside the usable
 * LBA range; logical partitions: inside their extended partition) are
 * skipped.
 *
 * Sector (LBA) size is the block size of the parent's interface
 * (ext4_blockdev_iface::ph_bsize).
 *
 * @param parent block device holding the partition table
 * @param bdevs  output, cleared first
 * @return EOK: bdevs holds the partitions
 *         ENOENT: no MBR signature (no partition table)
 *         EIO: invalid partition table (GPT: neither the primary nor the
 *              backup header with its entry array is valid; MBR: extended
 *              boot record outside its extended partition or the device,
 *              without signature, or chain loop / too long)
 *         ENOTSUP: sector size not a multiple of 128 bytes
 *         other: read error*/
int ext4_partition_scan(struct ext4_blockdev *parent,
			struct ext4_part_bdevs *bdevs);

/**@brief Scan an MBR partition table with its logical partitions.
 *        See ext4_partition_scan. Does not treat a protective MBR
 *        specially: it reports the 0xEE partition of a GPT disk.*/
int ext4_mbr_scan_all(struct ext4_blockdev *parent,
		      struct ext4_part_bdevs *bdevs);

/**@brief Scan a GPT (a protective MBR is required).
 *        See ext4_partition_scan.*/
int ext4_gpt_scan(struct ext4_blockdev *parent, struct ext4_part_bdevs *bdevs);

/**@brief Check whether a partition is a Linux filesystem partition: MBR type
 *        0x83 or GPT type EXT4_GPT_TYPE_LINUX_FS.*/
bool ext4_part_is_linux(const struct ext4_part_info *info);

#ifdef __cplusplus
}
#endif

#endif /* EXT4_PARTITION_H_ */

/**
 * @}
 */
