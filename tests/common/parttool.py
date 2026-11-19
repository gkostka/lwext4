# SPDX-License-Identifier: BSD-3-Clause
"""Patch the MBR/GPT partition tables of test disk images.

sfdisk writes valid partition tables; this tool damages or modifies them in
ways sfdisk cannot, recomputing the GPT CRC32s (zlib.crc32, independent of
lwext4) where the change should keep them valid.

  parttool.py [-b SECTOR] IMG gpt-header primary|backup FIELD=VALUE...
      Set header fields and recompute the header CRC (unless header_crc32
      itself is set).
  parttool.py [-b SECTOR] IMG gpt-entry primary|backup|both INDEX FIELD=VALUE...
      Set fields of partition entry INDEX (0 based) and recompute the entry
      array CRC and the header CRC. FIELD is first_lba, last_lba,
      attributes, name (text) or name_hex (raw UTF-16LE bytes).
  parttool.py [-b SECTOR] IMG gpt-entry-size SIZE
      Rewrite both entry arrays with SIZE byte entries (same array size in
      bytes, zero padded) and fix all CRCs.
  parttool.py [-b SECTOR] IMG mbr SLOT FIELD=VALUE...
  parttool.py [-b SECTOR] IMG ebr N SLOT FIELD=VALUE...
      Set fields (status, type, start, size) of partition record SLOT of the
      MBR or of the N-th (0 based) extended boot record of the chain of the
      first extended partition. FIELD may also be signature (the 0xAA55
      boot signature of that sector).
  parttool.py IMG flip OFFSET...
      Invert the byte at each OFFSET.
"""

import struct
import sys
import zlib

GPT_HEADER = {
    "revision": (8, "<I"),
    "header_size": (12, "<I"),
    "header_crc32": (16, "<I"),
    "my_lba": (24, "<Q"),
    "alternate_lba": (32, "<Q"),
    "first_usable_lba": (40, "<Q"),
    "last_usable_lba": (48, "<Q"),
    "entries_lba": (72, "<Q"),
    "num_entries": (80, "<I"),
    "entry_size": (84, "<I"),
    "entries_crc32": (88, "<I"),
}

GPT_ENTRY = {
    "first_lba": (32, "<Q"),
    "last_lba": (40, "<Q"),
    "attributes": (48, "<Q"),
}

MBR_RECORD = {
    "status": (0, "<B"),
    "type": (4, "<B"),
    "start": (8, "<I"),
    "size": (12, "<I"),
}

EXTENDED = (0x05, 0x0F, 0x85)


class Disk:
    def __init__(self, path, sector):
        self.path = path
        self.bs = sector
        with open(path, "rb") as f:
            self.data = bytearray(f.read())
        self.sectors = len(self.data) // sector

    def save(self):
        with open(self.path, "r+b") as f:
            f.write(self.data)

    def get(self, off, fmt):
        return struct.unpack_from(fmt, self.data, off)[0]

    def put(self, off, fmt, value):
        struct.pack_into(fmt, self.data, off, value)

    # GPT
    def header_lba(self, which):
        return 1 if which == "primary" else self.sectors - 1

    def fix_header_crc(self, lba):
        off = lba * self.bs
        size = self.get(off + 12, "<I")
        if not 92 <= size <= self.bs:
            size = 92
        self.put(off + 16, "<I", 0)
        crc = zlib.crc32(bytes(self.data[off:off + size]))
        self.put(off + 16, "<I", crc)

    def array(self, lba):
        off = lba * self.bs
        start = self.get(off + 72, "<Q") * self.bs
        size = self.get(off + 80, "<I") * self.get(off + 84, "<I")
        return start, size

    def fix_array_crc(self, lba):
        start, size = self.array(lba)
        crc = zlib.crc32(bytes(self.data[start:start + size]))
        self.put(lba * self.bs + 88, "<I", crc)
        self.fix_header_crc(lba)

    # MBR
    def ebr_lba(self, n):
        ext = None
        for slot in range(4):
            off = 446 + 16 * slot
            if self.data[off + 4] in EXTENDED:
                ext = self.get(off + 8, "<I")
                break
        if ext is None:
            sys.exit("no extended partition")
        lba = ext
        for _ in range(n):
            off = lba * self.bs + 446 + 16
            if self.data[off + 4] not in EXTENDED:
                sys.exit("EBR chain shorter than requested")
            lba = ext + self.get(off + 8, "<I")
        return lba


def parse_int(value):
    return int(value, 0)


def set_record(disk, lba, slot, assignments):
    for a in assignments:
        field, value = a.split("=", 1)
        if field == "signature":
            disk.put(lba * disk.bs + 510, "<H", parse_int(value))
            continue
        rel, fmt = MBR_RECORD[field]
        disk.put(lba * disk.bs + 446 + 16 * slot + rel, fmt, parse_int(value))


def main(argv):
    sector = 512
    if argv[:1] == ["-b"]:
        sector = int(argv[1])
        argv = argv[2:]
    path, cmd, args = argv[0], argv[1], argv[2:]

    if cmd == "flip":
        with open(path, "r+b") as f:
            for off in args:
                f.seek(parse_int(off))
                b = f.read(1)[0]
                f.seek(parse_int(off))
                f.write(bytes([b ^ 0xFF]))
        return

    disk = Disk(path, sector)

    if cmd == "gpt-header":
        lba = disk.header_lba(args[0])
        for a in args[1:]:
            field, value = a.split("=", 1)
            rel, fmt = GPT_HEADER[field]
            disk.put(lba * disk.bs + rel, fmt, parse_int(value))
        if not any(a.startswith("header_crc32=") for a in args[1:]):
            disk.fix_header_crc(lba)
    elif cmd == "gpt-entry":
        which = ["primary", "backup"] if args[0] == "both" else [args[0]]
        index = int(args[1])
        for w in which:
            lba = disk.header_lba(w)
            start, _ = disk.array(lba)
            esize = disk.get(lba * disk.bs + 84, "<I")
            off = start + index * esize
            for a in args[2:]:
                field, value = a.split("=", 1)
                if field == "name":
                    raw = value.encode("utf-16le")
                elif field == "name_hex":
                    raw = bytes.fromhex(value)
                else:
                    rel, fmt = GPT_ENTRY[field]
                    disk.put(off + rel, fmt, parse_int(value))
                    continue
                disk.data[off + 56:off + 128] = raw.ljust(72, b"\0")[:72]
            disk.fix_array_crc(lba)
    elif cmd == "gpt-entry-size":
        new = int(args[0])
        for w in ("primary", "backup"):
            lba = disk.header_lba(w)
            start, size = disk.array(lba)
            esize = disk.get(lba * disk.bs + 84, "<I")
            old = bytes(disk.data[start:start + size])
            entries = [old[i:i + 128] for i in range(0, size, esize)]
            count = size // new
            out = b"".join(e.ljust(new, b"\0") for e in entries[:count])
            disk.data[start:start + size] = out.ljust(size, b"\0")
            disk.put(lba * disk.bs + 80, "<I", count)
            disk.put(lba * disk.bs + 84, "<I", new)
            disk.fix_array_crc(lba)
    elif cmd == "mbr":
        set_record(disk, 0, int(args[0]), args[1:])
    elif cmd == "ebr":
        set_record(disk, disk.ebr_lba(int(args[0])), int(args[1]), args[2:])
    else:
        sys.exit("unknown command " + cmd)

    disk.save()


if __name__ == "__main__":
    main(sys.argv[1:])
