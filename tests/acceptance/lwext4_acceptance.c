/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * lwext4-acceptance: drive the public lwext4 API from a line based script so
 * the acceptance tests in this directory can exercise every documented
 * operation and then check the result with e2fsprogs (e2fsck, debugfs).
 *
 * Usage:
 *   lwext4-acceptance <image> [script]   run commands (stdin if no script)
 *   lwext4-acceptance --pattern <size> <seed> [offset]
 *                                        write the test data pattern for file
 *                                        offsets [offset, offset + size) to
 *                                        stdout
 *
 * Every command must succeed, otherwise the program prints the failing line
 * and exits with status 1. Paths are relative to the mount point ("/a/b" is
 * the file b in the directory a of the filesystem). Commands:
 *
 *   mount [ro]              ext4_device_register + ext4_mount
 *   umount                  ext4_umount + ext4_device_unregister
 *   recover                 ext4_recover
 *   journal_start / journal_stop
 *   cache_write_back 0|1    ext4_cache_write_back
 *   cache_flush             ext4_cache_flush
 *   crash_after <n>         power loss: the n+1th block write and everything
 *                           after it never reaches the image (exit status 0)
 *   crash                   power loss now
 *   writes                  print "writes <n>" (block device write calls)
 *   trace_writes 0|1        log every block device write to stderr
 *   bstat                   print the block device and block cache counters
 *   cache_check             fail if the block cache ever held more blocks
 *                           than it was configured for
 *   stats                   print ext4_mount_point_stats
 *   mkdir <p> / dir_rm <p> / dir_mv <p> <new>
 *   write <p> <size> <seed> [chunk]   create/truncate and write the pattern
 *   append <p> <size> <seed>          append (pattern continues at EOF)
 *   verify <p> <size> <seed>          size and content check
 *   size <p> <size>                   size check only
 *   truncate <p> <size>
 *   seek_verify <p> <off> <len> <seed> read <len> bytes at <off>
 *   remove <p> / link <p> <new> / rename <p> <new>
 *   symlink <target> <p> / readlink <p> <target>
 *   mknod <p> fifo|sock|chr|blk <dev>
 *   chmod <p> <octal> / mode <p> <octal>
 *   chown <p> <uid> <gid> / owner <p> <uid> <gid>
 *   atime|mtime|ctime <p> <t>, get_atime|get_mtime|get_ctime <p> <t>
 *   setxattr <p> <name> <value> / getxattr <p> <name> <value>
 *   removexattr <p> <name> / listxattr <p> <name,name,...|->
 *   ls <p> <name,name,...|->  exact, sorted directory listing (no . and ..)
 *   count <p> <n>           number of entries (no . and ..)
 *   exists <p> file|dir|symlink|chr|blk|fifo|sock
 *   missing <p>             the path must not exist
 *   fail <errno> <command...>   the command must fail with this error code
 *                           (number or ENOENT, ENOTSUP, EROFS, ...)
 *   io <command...>         run the command, print the number of block
 *                           device reads and writes it caused
 *   io_at_most|reads_at_most|writes_at_most <n> <command...>
 *                           ... and fail if the I/Os (reads + writes),
 *                           reads or writes exceed n
 *   echo <text...>
 */

#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include <ext4_inode.h>
#include <ext4_types.h>

#include "../../blockdev/linux/file_dev.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEV_NAME "acc_dev"
#define MP "/mp/"
#define MAX_ARGS 16
#define PATH_LEN 512

static const char *image;
static struct ext4_blockdev *bd;
static bool mounted;

static const char *cur_line;
static unsigned cur_lineno;

static uint64_t write_calls;
static int64_t crash_limit = -1;
static bool trace_writes;
static int (*orig_bwrite)(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt);

static void die(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "lwext4-acceptance: line %u: %s\n  ", cur_lineno,
		cur_line ? cur_line : "");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

#define CHECK_RC(expr)                                                         \
	do {                                                                   \
		int rc_ = (expr);                                              \
		if (rc_ != EOK)                                                \
			die("%s failed: %d (%s)", #expr, rc_, strerror(rc_));  \
	} while (0)

/* Pattern byte at file offset off; identical on every host (no byte order
 * or word size dependence), so --pattern output can be compared with data
 * read back by debugfs on the build host. */
static uint8_t pattern_byte(uint32_t seed, uint64_t off)
{
	uint32_t v = (uint32_t)off ^ (uint32_t)(off >> 32) * 0x27d4eb2dU;

	v ^= seed * 0x9e3779b9U;
	v *= 0x85ebca6bU;
	v ^= v >> 13;
	v *= 0xc2b2ae35U;
	v ^= v >> 16;
	return (uint8_t)v;
}

static void pattern_fill(uint8_t *buf, size_t len, uint32_t seed, uint64_t off)
{
	size_t i;

	for (i = 0; i < len; i++)
		buf[i] = pattern_byte(seed, off + i);
}

static int counting_bwrite(struct ext4_blockdev *bdev, const void *buf,
			   uint64_t blk_id, uint32_t blk_cnt)
{
	if (crash_limit >= 0 && write_calls >= (uint64_t)crash_limit) {
		/* Power loss: nothing from here on reaches the image. The file
		 * device is unbuffered, so everything before is on "disk". */
		printf("crash after %" PRIu64 " writes, in line %u: %s\n",
		       write_calls, cur_lineno, cur_line);
		fflush(stdout);
		_exit(0);
	}
	write_calls++;
	if (trace_writes)
		fprintf(stderr, "write %" PRIu64 ": sector %" PRIu64
			" count %" PRIu32 " (line %u)\n",
			write_calls, blk_id, blk_cnt, cur_lineno);
	return orig_bwrite(bdev, buf, blk_id, blk_cnt);
}

static const char *fs_path(const char *p)
{
	static char buf[4][PATH_LEN];
	static int idx;
	char *out = buf[idx++ & 3];

	while (*p == '/')
		p++;
	if (snprintf(out, PATH_LEN, MP "%s", p) >= PATH_LEN)
		die("path too long");
	return out;
}

static unsigned long long num(const char *s)
{
	char *end;
	unsigned long long v;

	errno = 0;
	v = strtoull(s, &end, 0);
	if (errno || *end)
		die("bad number '%s'", s);
	return v;
}

static void need(int argc, int n)
{
	if (argc != n)
		die("expected %d arguments, got %d", n - 1, argc - 1);
}

static int do_write(const char *path, uint64_t size, uint32_t seed,
		    size_t chunk, const char *flags)
{
	ext4_file f;
	uint8_t *buf;
	uint64_t off, start;
	size_t n, wcnt;
	int r;

	r = ext4_fopen(&f, path, flags);
	if (r != EOK)
		return r;
	start = ext4_fsize(&f);
	buf = malloc(chunk);
	if (!buf)
		die("out of memory");
	for (off = 0; off < size; off += n) {
		n = size - off < chunk ? (size_t)(size - off) : chunk;
		pattern_fill(buf, n, seed, start + off);
		r = ext4_fwrite(&f, buf, n, &wcnt);
		if (r != EOK)
			break;
		if (wcnt != n)
			die("short write: %zu of %zu at %" PRIu64, wcnt, n,
			    start + off);
	}
	free(buf);
	if (r != EOK) {
		ext4_fclose(&f);
		return r;
	}
	return ext4_fclose(&f);
}

static int do_read_verify(const char *path, uint64_t off, uint64_t len,
			  uint32_t seed, int64_t exp_size)
{
	ext4_file f;
	uint8_t *buf, *exp;
	const size_t chunk = 65536 + 123; /* deliberately not block aligned */
	uint64_t pos;
	size_t n, rcnt, i;
	int r;

	r = ext4_fopen(&f, path, "rb");
	if (r != EOK)
		return r;
	if (exp_size >= 0 && ext4_fsize(&f) != (uint64_t)exp_size)
		die("%s: size %" PRIu64 ", expected %" PRId64, path,
		    ext4_fsize(&f), exp_size);
	r = ext4_fseek(&f, (int64_t)off, SEEK_SET);
	if (r != EOK)
		die("fseek(%" PRIu64 "): %d", off, r);
	buf = malloc(chunk);
	exp = malloc(chunk);
	if (!buf || !exp)
		die("out of memory");
	for (pos = 0; pos < len; pos += n) {
		n = len - pos < chunk ? (size_t)(len - pos) : chunk;
		r = ext4_fread(&f, buf, n, &rcnt);
		if (r != EOK)
			die("%s: ext4_fread at %" PRIu64 ": %d", path, off + pos,
			    r);
		if (rcnt != n)
			die("%s: short read at %" PRIu64 ": %zu of %zu", path,
			    off + pos, rcnt, n);
		pattern_fill(exp, n, seed, off + pos);
		if (memcmp(buf, exp, n)) {
			for (i = 0; buf[i] == exp[i]; i++)
				;
			die("%s: data mismatch at offset %" PRIu64, path,
			    off + pos + i);
		}
	}
	/* Reading at EOF must return 0 bytes. */
	if (exp_size >= 0) {
		r = ext4_fread(&f, buf, 1, &rcnt);
		if (r != EOK || rcnt != 0)
			die("%s: read past EOF returned rc %d, %zu bytes", path,
			    r, rcnt);
	}
	free(buf);
	free(exp);
	return ext4_fclose(&f);
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Sorted, comma separated directory listing without "." and "..". */
static char *list_dir(const char *path, size_t *count)
{
	ext4_dir d;
	const ext4_direntry *de;
	char **names = NULL;
	size_t n = 0, cap = 0, len = 1, i;
	char *out;
	int r;

	r = ext4_dir_open(&d, path);
	if (r != EOK)
		die("ext4_dir_open(%s): %d", path, r);
	while ((de = ext4_dir_entry_next(&d)) != NULL) {
		if ((de->name_length == 1 && de->name[0] == '.') ||
		    (de->name_length == 2 && !memcmp(de->name, "..", 2)))
			continue;
		if (n == cap) {
			cap = cap ? cap * 2 : 64;
			names = realloc(names, cap * sizeof(*names));
			if (!names)
				die("out of memory");
		}
		names[n] = malloc(de->name_length + 1);
		if (!names[n])
			die("out of memory");
		memcpy(names[n], de->name, de->name_length);
		names[n][de->name_length] = 0;
		len += de->name_length + 1;
		n++;
	}
	CHECK_RC(ext4_dir_close(&d));
	if (n)
		qsort(names, n, sizeof(*names), cmp_str);
	out = malloc(len + 1);
	if (!out)
		die("out of memory");
	out[0] = 0;
	for (i = 0; i < n; i++) {
		if (i)
			strcat(out, ",");
		strcat(out, names[i]);
		free(names[i]);
	}
	free(names);
	if (!n)
		strcpy(out, "-");
	*count = n;
	return out;
}

static int type_of(const char *s)
{
	static const struct {
		const char *name;
		int type;
	} types[] = {
	    {"file", EXT4_DE_REG_FILE}, {"dir", EXT4_DE_DIR},
	    {"symlink", EXT4_DE_SYMLINK}, {"chr", EXT4_DE_CHRDEV},
	    {"blk", EXT4_DE_BLKDEV}, {"fifo", EXT4_DE_FIFO},
	    {"sock", EXT4_DE_SOCK},
	};
	size_t i;

	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		if (!strcmp(s, types[i].name))
			return types[i].type;
	die("unknown file type '%s'", s);
	return -1;
}

static int run(int argc, char **argv);

static int errno_value(const char *s)
{
	static const struct {
		const char *name;
		int value;
	} names[] = {
	    {"ENOENT", ENOENT}, {"EIO", EIO},	  {"EEXIST", EEXIST},
	    {"ENOTDIR", ENOTDIR}, {"EISDIR", EISDIR}, {"EINVAL", EINVAL},
	    {"ENOSPC", ENOSPC}, {"EROFS", EROFS},	  {"ENOTEMPTY", ENOTEMPTY},
	    {"ENOTSUP", ENOTSUP}, {"ENODATA", ENODATA},
	};
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (!strcmp(s, names[i].name))
			return names[i].value;
	return (int)num(s);
}

static int cmd_time(int argc, char **argv)
{
	const char *p;
	uint32_t t, got;
	int r;

	need(argc, 3);
	p = fs_path(argv[1]);
	t = (uint32_t)num(argv[2]);
	if (!strcmp(argv[0], "atime"))
		return ext4_atime_set(p, t);
	if (!strcmp(argv[0], "mtime"))
		return ext4_mtime_set(p, t);
	if (!strcmp(argv[0], "ctime"))
		return ext4_ctime_set(p, t);
	if (!strcmp(argv[0], "get_atime"))
		r = ext4_atime_get(p, &got);
	else if (!strcmp(argv[0], "get_mtime"))
		r = ext4_mtime_get(p, &got);
	else
		r = ext4_ctime_get(p, &got);
	if (r == EOK && got != t)
		die("%s: %" PRIu32 ", expected %" PRIu32, argv[0], got, t);
	return r;
}

static int run(int argc, char **argv)
{
	const char *c = argv[0];

	if (!strcmp(c, "mount")) {
		bool ro = argc == 2 && !strcmp(argv[1], "ro");
		int r;

		if (argc > 2 || (argc == 2 && !ro))
			die("usage: mount [ro]");
		file_dev_name_set(image);
		bd = file_dev_get();
		if (!orig_bwrite) {
			orig_bwrite = bd->bdif->bwrite;
			bd->bdif->bwrite = counting_bwrite;
		}
		r = ext4_device_register(bd, DEV_NAME);
		if (r != EOK)
			return r;
		r = ext4_mount(DEV_NAME, MP, ro);
		if (r != EOK) {
			ext4_device_unregister(DEV_NAME);
			return r;
		}
		mounted = true;
		return EOK;
	}
	if (!strcmp(c, "umount")) {
		int r;

		need(argc, 1);
		r = ext4_umount(MP);
		if (r != EOK)
			return r;
		mounted = false;
		return ext4_device_unregister(DEV_NAME);
	}
	if (!strcmp(c, "recover")) {
		need(argc, 1);
		return ext4_recover(MP);
	}
	if (!strcmp(c, "journal_start")) {
		need(argc, 1);
		return ext4_journal_start(MP);
	}
	if (!strcmp(c, "journal_stop")) {
		need(argc, 1);
		return ext4_journal_stop(MP);
	}
	if (!strcmp(c, "cache_write_back")) {
		need(argc, 2);
		return ext4_cache_write_back(MP, num(argv[1]) != 0);
	}
	if (!strcmp(c, "cache_flush")) {
		need(argc, 1);
		return ext4_cache_flush(MP);
	}
	if (!strcmp(c, "crash_after")) {
		need(argc, 2);
		crash_limit = (int64_t)(write_calls + num(argv[1]));
		return EOK;
	}
	if (!strcmp(c, "crash")) {
		need(argc, 1);
		printf("crash after %" PRIu64 " writes\n", write_calls);
		fflush(stdout);
		_exit(0);
	}
	if (!strcmp(c, "trace_writes")) {
		need(argc, 2);
		trace_writes = num(argv[1]) != 0;
		return EOK;
	}
	if (!strcmp(c, "writes")) {
		need(argc, 1);
		printf("writes %" PRIu64 "\n", write_calls);
		return EOK;
	}
	if (!strcmp(c, "bstat") || !strcmp(c, "cache_check")) {
		need(argc, 1);
		if (!bd || !bd->bc)
			die("not mounted");
		printf("bread_ctr %" PRIu32 " bwrite_ctr %" PRIu32
		       " cache_size %" PRIu32 " max_ref_blocks %" PRIu32 "\n",
		       bd->bdif->bread_ctr, bd->bdif->bwrite_ctr, bd->bc->cnt,
		       bd->bc->max_ref_blocks);
		if (!strcmp(c, "cache_check") &&
		    bd->bc->max_ref_blocks > bd->bc->cnt)
			die("block cache held %" PRIu32 " blocks, configured "
			    "for %" PRIu32, bd->bc->max_ref_blocks, bd->bc->cnt);
		return EOK;
	}
	if (!strcmp(c, "stats")) {
		struct ext4_mount_stats s;

		need(argc, 1);
		CHECK_RC(ext4_mount_point_stats(MP, &s));
		printf("inodes_count %" PRIu32 " free_inodes_count %" PRIu32
		       " blocks_count %" PRIu64 " free_blocks_count %" PRIu64
		       " block_size %" PRIu32 " block_group_count %" PRIu32
		       " volume_name %.16s\n",
		       s.inodes_count, s.free_inodes_count, s.blocks_count,
		       s.free_blocks_count, s.block_size, s.block_group_count,
		       s.volume_name);
		return EOK;
	}
	if (!strcmp(c, "mkdir")) {
		need(argc, 2);
		return ext4_dir_mk(fs_path(argv[1]));
	}
	if (!strcmp(c, "dir_rm")) {
		need(argc, 2);
		return ext4_dir_rm(fs_path(argv[1]));
	}
	if (!strcmp(c, "dir_mv")) {
		need(argc, 3);
		return ext4_dir_mv(fs_path(argv[1]), fs_path(argv[2]));
	}
	if (!strcmp(c, "write")) {
		size_t chunk = 65536;

		if (argc == 5)
			chunk = (size_t)num(argv[4]);
		else
			need(argc, 4);
		if (!chunk)
			die("chunk must not be 0");
		return do_write(fs_path(argv[1]), num(argv[2]),
				(uint32_t)num(argv[3]), chunk, "wb");
	}
	if (!strcmp(c, "append")) {
		need(argc, 4);
		return do_write(fs_path(argv[1]), num(argv[2]),
				(uint32_t)num(argv[3]), 4099, "ab");
	}
	if (!strcmp(c, "verify")) {
		uint64_t size;

		need(argc, 4);
		size = num(argv[2]);
		return do_read_verify(fs_path(argv[1]), 0, size,
				      (uint32_t)num(argv[3]), (int64_t)size);
	}
	if (!strcmp(c, "size")) {
		ext4_file f;
		int r;

		need(argc, 3);
		r = ext4_fopen(&f, fs_path(argv[1]), "rb");
		if (r != EOK)
			return r;
		if (ext4_fsize(&f) != num(argv[2]))
			die("size %" PRIu64 ", expected %s", ext4_fsize(&f),
			    argv[2]);
		return ext4_fclose(&f);
	}
	if (!strcmp(c, "seek_verify")) {
		need(argc, 5);
		return do_read_verify(fs_path(argv[1]), num(argv[2]),
				      num(argv[3]), (uint32_t)num(argv[4]), -1);
	}
	if (!strcmp(c, "truncate")) {
		ext4_file f;
		int r;

		need(argc, 3);
		r = ext4_fopen(&f, fs_path(argv[1]), "rb+");
		if (r != EOK)
			return r;
		r = ext4_ftruncate(&f, num(argv[2]));
		if (r != EOK) {
			ext4_fclose(&f);
			return r;
		}
		if (ext4_fsize(&f) != num(argv[2]))
			die("size after truncate: %" PRIu64, ext4_fsize(&f));
		return ext4_fclose(&f);
	}
	if (!strcmp(c, "remove")) {
		need(argc, 2);
		return ext4_fremove(fs_path(argv[1]));
	}
	if (!strcmp(c, "link")) {
		need(argc, 3);
		return ext4_flink(fs_path(argv[1]), fs_path(argv[2]));
	}
	if (!strcmp(c, "rename")) {
		need(argc, 3);
		return ext4_frename(fs_path(argv[1]), fs_path(argv[2]));
	}
	if (!strcmp(c, "symlink")) {
		need(argc, 3);
		return ext4_fsymlink(argv[1], fs_path(argv[2]));
	}
	if (!strcmp(c, "readlink")) {
		char buf[4096];
		size_t rcnt;
		int r;

		need(argc, 3);
		memset(buf, 0, sizeof(buf));
		r = ext4_readlink(fs_path(argv[1]), buf, sizeof(buf) - 1,
				  &rcnt);
		if (r != EOK)
			return r;
		if (rcnt != strlen(argv[2]) || memcmp(buf, argv[2], rcnt))
			die("readlink: '%.*s', expected '%s'", (int)rcnt, buf,
			    argv[2]);
		return EOK;
	}
	if (!strcmp(c, "mknod")) {
		need(argc, 4);
		return ext4_mknod(fs_path(argv[1]), type_of(argv[2]),
				  (uint32_t)num(argv[3]));
	}
	if (!strcmp(c, "chmod")) {
		need(argc, 3);
		return ext4_mode_set(fs_path(argv[1]),
				     (uint32_t)strtoul(argv[2], NULL, 8));
	}
	if (!strcmp(c, "mode")) {
		uint32_t mode, exp;
		int r;

		need(argc, 3);
		exp = (uint32_t)strtoul(argv[2], NULL, 8);
		r = ext4_mode_get(fs_path(argv[1]), &mode);
		if (r == EOK && (mode & 07777) != exp)
			die("mode %o, expected %o", mode & 07777, exp);
		return r;
	}
	if (!strcmp(c, "chown")) {
		need(argc, 4);
		return ext4_owner_set(fs_path(argv[1]), (uint32_t)num(argv[2]),
				      (uint32_t)num(argv[3]));
	}
	if (!strcmp(c, "owner")) {
		uint32_t uid, gid;
		int r;

		need(argc, 4);
		r = ext4_owner_get(fs_path(argv[1]), &uid, &gid);
		if (r == EOK && (uid != num(argv[2]) || gid != num(argv[3])))
			die("owner %" PRIu32 ":%" PRIu32, uid, gid);
		return r;
	}
	if (!strcmp(c, "atime") || !strcmp(c, "mtime") || !strcmp(c, "ctime") ||
	    !strcmp(c, "get_atime") || !strcmp(c, "get_mtime") ||
	    !strcmp(c, "get_ctime"))
		return cmd_time(argc, argv);
	if (!strcmp(c, "setxattr")) {
		need(argc, 4);
		return ext4_setxattr(fs_path(argv[1]), argv[2],
				     strlen(argv[2]), argv[3], strlen(argv[3]));
	}
	if (!strcmp(c, "getxattr")) {
		char buf[4096];
		size_t len;
		int r;

		need(argc, 4);
		r = ext4_getxattr(fs_path(argv[1]), argv[2], strlen(argv[2]),
				  buf, sizeof(buf), &len);
		if (r != EOK)
			return r;
		if (len != strlen(argv[3]) || memcmp(buf, argv[3], len))
			die("xattr %s: '%.*s', expected '%s'", argv[2],
			    (int)len, buf, argv[3]);
		return EOK;
	}
	if (!strcmp(c, "removexattr")) {
		need(argc, 3);
		return ext4_removexattr(fs_path(argv[1]), argv[2],
					strlen(argv[2]));
	}
	if (!strcmp(c, "listxattr")) {
		char buf[4096], out[4096];
		char *names[64];
		size_t len, n = 0, i;
		char *p;
		int r;

		need(argc, 3);
		memset(buf, 0, sizeof(buf));
		r = ext4_listxattr(fs_path(argv[1]), buf, sizeof(buf) - 1,
				   &len);
		if (r != EOK)
			return r;
		for (p = buf; p < buf + len && *p && n < 64; p += strlen(p) + 1)
			names[n++] = p;
		qsort(names, n, sizeof(names[0]), cmp_str);
		out[0] = 0;
		for (i = 0; i < n; i++) {
			if (i)
				strcat(out, ",");
			strcat(out, names[i]);
		}
		if (!n)
			strcpy(out, "-");
		if (strcmp(out, argv[2]))
			die("listxattr: '%s', expected '%s'", out, argv[2]);
		return EOK;
	}
	if (!strcmp(c, "ls") || !strcmp(c, "count")) {
		size_t n;
		char *l;

		need(argc, 3);
		l = list_dir(fs_path(argv[1]), &n);
		if (!strcmp(c, "ls") && strcmp(l, argv[2]))
			die("ls: '%s', expected '%s'", l, argv[2]);
		if (!strcmp(c, "count") && n != num(argv[2]))
			die("count: %zu entries, expected %s", n, argv[2]);
		free(l);
		return EOK;
	}
	if (!strcmp(c, "exists")) {
		need(argc, 3);
		return ext4_inode_exist(fs_path(argv[1]), type_of(argv[2]));
	}
	if (!strcmp(c, "missing")) {
		uint32_t ino;
		struct ext4_inode inode;
		int r;

		need(argc, 2);
		r = ext4_raw_inode_fill(fs_path(argv[1]), &ino, &inode);
		if (r == EOK)
			die("%s exists (inode %" PRIu32 ")", argv[1], ino);
		return r == ENOENT ? EOK : r;
	}
	if (!strcmp(c, "io") || !strcmp(c, "io_at_most") ||
	    !strcmp(c, "reads_at_most") || !strcmp(c, "writes_at_most")) {
		uint32_t r0, w0, reads, writes, count;
		int skip = !strcmp(c, "io") ? 1 : 2;
		int r;

		if (argc <= skip)
			die("usage: %s [n] <command...>", c);
		if (!bd)
			die("not mounted");
		r0 = bd->bdif->bread_ctr;
		w0 = bd->bdif->bwrite_ctr;
		r = run(argc - skip, argv + skip);
		reads = bd->bdif->bread_ctr - r0;
		writes = bd->bdif->bwrite_ctr - w0;
		printf("io %s: reads %" PRIu32 " writes %" PRIu32 "\n",
		       argv[skip], reads, writes);
		if (!strcmp(c, "reads_at_most"))
			count = reads;
		else if (!strcmp(c, "writes_at_most"))
			count = writes;
		else
			count = reads + writes;
		if (skip == 2 && count > num(argv[1]))
			die("%s: %" PRIu32 " (reads %" PRIu32 ", writes %" PRIu32
			    "), expected at most %s",
			    c, count, reads, writes, argv[1]);
		return r;
	}
	if (!strcmp(c, "fail")) {
		int want, r;

		if (argc < 3)
			die("usage: fail <errno> <command...>");
		want = errno_value(argv[1]);
		r = run(argc - 2, argv + 2);
		if (r != want)
			die("expected error %d (%s), got %d (%s)", want,
			    strerror(want), r, strerror(r));
		return EOK;
	}
	if (!strcmp(c, "echo")) {
		int i;

		for (i = 1; i < argc; i++)
			printf("%s%s", i > 1 ? " " : "", argv[i]);
		putchar('\n');
		return EOK;
	}
	die("unknown command '%s'", c);
	return EINVAL;
}

static int print_pattern(int argc, char **argv)
{
	uint8_t buf[65536];
	uint64_t size, off, start = 0;
	uint32_t seed;
	size_t n;

	if (argc != 4 && argc != 5) {
		fprintf(stderr, "usage: %s --pattern <size> <seed> [offset]\n",
			argv[0]);
		return 2;
	}
	size = num(argv[2]);
	seed = (uint32_t)num(argv[3]);
	if (argc == 5)
		start = num(argv[4]);
	for (off = 0; off < size; off += n) {
		n = size - off < sizeof(buf) ? (size_t)(size - off)
					     : sizeof(buf);
		pattern_fill(buf, n, seed, start + off);
		if (fwrite(buf, 1, n, stdout) != n)
			return 1;
	}
	return fflush(stdout) ? 1 : 0;
}

int main(int argc, char **argv)
{
	char line[4096], copy[4096];
	char *args[MAX_ARGS];
	FILE *in = stdin;
	int n, r;
	char *tok, *save;

	if (argc >= 2 && !strcmp(argv[1], "--pattern"))
		return print_pattern(argc, argv);
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s <image> [script]\n"
				"       %s --pattern <size> <seed>\n",
			argv[0], argv[0]);
		return 2;
	}
	image = argv[1];
	if (argc == 3) {
		in = fopen(argv[2], "r");
		if (!in) {
			perror(argv[2]);
			return 2;
		}
	}
	setvbuf(stdout, NULL, _IOLBF, 0);

	while (fgets(line, sizeof(line), in)) {
		cur_lineno++;
		line[strcspn(line, "\r\n")] = 0;
		strcpy(copy, line);
		cur_line = copy;
		n = 0;
		for (tok = strtok_r(line, " \t", &save); tok && n < MAX_ARGS;
		     tok = strtok_r(NULL, " \t", &save))
			args[n++] = tok;
		if (!n || args[0][0] == '#')
			continue;
		r = run(n, args);
		if (r != EOK)
			die("failed: %d (%s)", r, strerror(r));
	}
	if (mounted)
		die("script ended with the filesystem still mounted");
	return 0;
}
