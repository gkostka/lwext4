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

#ifdef WIN32
#include <windows.h>
#include <winioctl.h>

/**@brief   Default filename.*/
static const char *fname = "ext2";

/**@brief   IO block size.*/
#define EXT4_IORAW_BSIZE 512

/**@brief   Image file descriptor.*/
static HANDLE dev_file;

/**********************BLOCKDEV INTERFACE**************************************/
static int file_open(struct ext4_blockdev *bdev);
static int file_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			uint32_t blk_cnt);
static int file_bwrite(struct ext4_blockdev *bdev, const void *buf,
			 uint64_t blk_id, uint32_t blk_cnt);
static int file_close(struct ext4_blockdev *bdev);

/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(_filedev, EXT4_IORAW_BSIZE, 0, file_open,
			      file_bread, file_bwrite, file_close, 0, 0);

/******************************************************************************/
static int file_open(struct ext4_blockdev *bdev)
{
	char path[64];
	DISK_GEOMETRY pdg;
	uint64_t disk_size;
	BOOL bResult = FALSE;
	DWORD junk;

	sprintf(path, "\\\\.\\%s", fname);

	dev_file =
	    CreateFile(path, GENERIC_READ | GENERIC_WRITE,
		       FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
		       FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

	if (dev_file == INVALID_HANDLE_VALUE) {
		return EIO;
	}

	bResult =
	    DeviceIoControl(dev_file, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
			    &pdg, sizeof(pdg), &junk, (LPOVERLAPPED)NULL);

	if (bResult == FALSE) {
		CloseHandle(dev_file);
		return EIO;
	}

	disk_size = pdg.Cylinders.QuadPart * (ULONG)pdg.TracksPerCylinder *
		    (ULONG)pdg.SectorsPerTrack * (ULONG)pdg.BytesPerSector;

	_filedev.bdif->ph_bsize = pdg.BytesPerSector;
	_filedev.bdif->ph_bcnt = disk_size / pdg.BytesPerSector;

	_filedev.part_offset = 0;
	_filedev.part_size = disk_size;

	return EOK;
}

/******************************************************************************/

static int file_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			uint32_t blk_cnt)
{
	long hipart = blk_id >> (32 - 9);
	long lopart = blk_id << 9;
	long err;

	SetLastError(0);
	lopart = SetFilePointer(dev_file, lopart, &hipart, FILE_BEGIN);

	if (lopart == -1 && NO_ERROR != (err = GetLastError())) {
		return EIO;
	}

	DWORD n;

	if (!ReadFile(dev_file, buf, blk_cnt * 512, &n, NULL)) {
		err = GetLastError();
		return EIO;
	}
	return EOK;
}

/******************************************************************************/
static int file_bwrite(struct ext4_blockdev *bdev, const void *buf,
			 uint64_t blk_id, uint32_t blk_cnt)
{
	long hipart = blk_id >> (32 - 9);
	long lopart = blk_id << 9;
	long err;

	SetLastError(0);
	lopart = SetFilePointer(dev_file, lopart, &hipart, FILE_BEGIN);

	if (lopart == -1 && NO_ERROR != (err = GetLastError())) {
		return EIO;
	}

	DWORD n;

	if (!WriteFile(dev_file, buf, blk_cnt * 512, &n, NULL)) {
		err = GetLastError();
		return EIO;
	}
	return EOK;
}

/******************************************************************************/
static int file_close(struct ext4_blockdev *bdev)
{
	CloseHandle(dev_file);
	return EOK;
}

/******************************************************************************/
struct ext4_blockdev *file_windows_dev_get(void)
{
	return &_filedev;
}
/******************************************************************************/
void file_windows_name_set(const char *n)
{
	fname = n;
}

/******************************************************************************/
#endif
