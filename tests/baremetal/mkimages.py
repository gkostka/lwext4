#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build test data for tests/baremetal with e2fsprogs and emit a C header.

Usage: mkimages.py <work dir> <output header> [image name...]

Without image names all images below are built.

Directory hash reference values: debugfs dx_hash for every hash version and
a few names (including bytes >= 0x80, where the signed and unsigned variants
differ), with the default and a fixed hash seed.

Images:

The images are populated with mke2fs -d, their directories are indexed with
e2fsck -D and they must pass e2fsck -fn. Only the non-zero 512 byte sectors
are emitted, deduplicated (sorted sector numbers, the index of their data and
the distinct sector data), so the images fit into the flash of small
microcontrollers. images.c mounts them read-only and
checks the content described below; keep both in sync.

  /hello.txt   "hello lwext4\\n"
  /data.bin    DATA_SIZE bytes, byte i = (i * 7 + (i >> 9)) & 0xff
  /dir/        DIR_FILES empty files named NAME_FMT % i (long names, so the
               directory spans several blocks and gets an htree index)
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

DATA_SIZE = {"ext2": 12 * 1024 + 100, "ext4": 3 * 1024 + 100}
DIR_FILES = 24
SECTOR = 512
NAME_FMT = ("file_with_a_very_long_name_to_fill_directory_blocks_quickly_and_"
            "to_hash_more_than_one_md4_or_tea_chunk_%02d")

# name, mke2fs arguments. 1 KiB blocks everywhere; ext2 has 12 direct
# blocks, so its data.bin needs an indirect block. The ext4 image keeps
# metadata_csum, extents, 64bit, flex_bg and dir_index but drops the
# journal (read-only mount only, and it would just be zeroes).
IMAGES = [
    ("ext2", ["-t", "ext2", "-b", "1024", "-I", "128", "-N", "64",
              "-m", "0", "-O", "^resize_inode"], "160K"),
    ("ext4", ["-t", "ext4", "-b", "1024", "-I", "128", "-N", "64",
              "-m", "0", "-O", "^has_journal,^resize_inode"], "160K"),
]
# Newer e2fsprogs enable features lwext4 does not support.
NEWER_FEATURES = ["-O", "^orphan_file,^metadata_csum_seed"]


def run(cmd, ok=(0,)):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode not in ok:
        sys.stdout.write(r.stdout.decode(errors="replace"))
        raise SystemExit("%s failed with %d" % (" ".join(cmd), r.returncode))
    return r.returncode


def populate(root, name):
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(os.path.join(root, "dir"))
    with open(os.path.join(root, "hello.txt"), "w") as f:
        f.write("hello lwext4\n")
    with open(os.path.join(root, "data.bin"), "wb") as f:
        f.write(bytes(((i * 7 + (i >> 9)) & 0xff)
                      for i in range(DATA_SIZE[name])))
    for i in range(DIR_FILES):
        open(os.path.join(root, "dir", NAME_FMT % i),
             "w").close()


def build(work, name, args, size):
    # mke2fs -d copies extended attributes (e.g. default POSIX ACLs of the
    # build directory), so populate a fresh directory in the temp dir.
    root = os.path.join(tempfile.mkdtemp(prefix="lwext4-img-"), name)
    img = os.path.join(work, name + ".img")
    populate(root, name)
    if os.path.exists(img):
        os.remove(img)
    base = ["mke2fs", "-q", "-F", "-d", root]
    r = subprocess.run(base + NEWER_FEATURES + args + [img, size],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if r.returncode != 0:
        run(base + args + [img, size])
    # Index the directories (mke2fs -d does not build htrees).
    run(["e2fsck", "-fyD", img], ok=(0, 1))
    run(["e2fsck", "-fn", img])
    shutil.rmtree(os.path.dirname(root))
    with open(img, "rb") as f:
        return f.read()


def emit(out, name, data):
    zero = bytes(SECTOR)
    sectors = [(n, data[n * SECTOR:(n + 1) * SECTOR])
               for n in range(len(data) // SECTOR)]
    sectors = [(n, s) for n, s in sectors if s != zero]
    unique = []
    ids = {}
    for _, s in sectors:
        if s not in ids:
            ids[s] = len(unique)
            unique.append(s)
    out.write("/* %s: %u bytes, %u non-zero sectors, %u distinct */\n"
              % (name, len(data), len(sectors), len(unique)))
    out.write("static const uint16_t img_%s_idx[][2] IMG_MEM = {\n" % name)
    for i in range(0, len(sectors), 6):
        out.write("\t" + " ".join("{%u, %u}," % (n, ids[s])
                                   for n, s in sectors[i:i + 6]) + "\n")
    out.write("};\n")
    out.write("static const uint8_t img_%s_data[][%u] IMG_MEM = {\n"
              % (name, SECTOR))
    for s in unique:
        out.write("\t{\n")
        for i in range(0, SECTOR, 16):
            out.write("\t\t" + ", ".join("0x%02x" % b for b in s[i:i + 16])
                      + ",\n")
        out.write("\t},\n")
    out.write("};\n\n")
    return len(sectors)


# EXT2_HTREE_* 0-5; e2p only knows the names of the signed variants.
HASH_ALGS = ["legacy", "half_md4", "tea", "HASHALG_3", "HASHALG_4",
             "HASHALG_5"]
HASH_NAMES = [b"hello", b"lost+found",
              b"a_name_that_is_longer_than_thirty_two_bytes_for_md4.txt",
              b"\xe9t\xe9-\xc3\xa9\xff\x80"]
HASH_SEED = "00112233-4455-6677-8899-aabbccddeeff"


def c_string(b):
    return '"' + "".join(chr(c) if 0x20 <= c < 0x7f and c not in b'"\\?'
                         else "\\%03o" % c for c in b) + '"'


def emit_hashes(out):
    out.write("#define HASH_SEED_BYTES {%s}\n" % ", ".join(
        "0x%02x" % b for b in bytes.fromhex(HASH_SEED.replace("-", ""))))
    out.write("/* X(name, hash version, seeded, major, minor) */\n")
    out.write("#define HASH_LIST(X) \\\n")
    for name in HASH_NAMES:
        for version, alg in enumerate(HASH_ALGS):
            for seeded in (0, 1):
                cmd = ["debugfs", "-R", b"dx_hash -h " + alg.encode() +
                       (b" -s " + HASH_SEED.encode() if seeded else b"") +
                       b" " + name, "/dev/null"]
                r = subprocess.run(cmd, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL)
                m = re.search(rb"is (0x[0-9a-f]+) \(minor (0x[0-9a-f]+)\)",
                              r.stdout)
                if not m:
                    raise SystemExit("debugfs dx_hash failed: %r" % r.stdout)
                out.write("\tX(%s, %u, %u, %sUL, %sUL) \\\n"
                          % (c_string(name), version, seeded,
                             m.group(1).decode(), m.group(2).decode()))
    out.write("\n")


def main():
    work, header = sys.argv[1], sys.argv[2]
    wanted = sys.argv[3:] or [name for name, _, _ in IMAGES]
    unknown = set(wanted) - set(name for name, _, _ in IMAGES)
    if unknown:
        raise SystemExit("unknown image(s): %s" % " ".join(sorted(unknown)))
    os.makedirs(work, exist_ok=True)
    os.environ["PATH"] += ":/sbin:/usr/sbin"
    tmp = header + ".tmp"
    with open(tmp, "w") as out:
        out.write("/* Generated by mkimages.py, do not edit. */\n\n")
        out.write("#define IMG_DIR_FILES %u\n" % DIR_FILES)
        out.write("#define IMG_NAME_FMT \"%s\"\n\n" % NAME_FMT)
        emit_hashes(out)
        # The images are only compiled where IMG_MEM is defined (images.c).
        out.write("#ifdef IMG_MEM\n\n")
        entries = []
        for name, args, size in IMAGES:
            if name not in wanted:
                continue
            data = build(work, name, args, size)
            count = emit(out, name, data)
            entries.append("X(%s, %uUL, %u, %uUL)"
                           % (name, len(data), count, DATA_SIZE[name]))
        out.write("/* X(name, image size, sectors, data.bin size) */\n")
        out.write("#define IMG_LIST(X) \\\n\t"
                  + " \\\n\t".join(entries) + "\n")
        out.write("\n#endif /* IMG_MEM */\n")
    os.replace(tmp, header)


if __name__ == "__main__":
    main()
