# Blank disks for ext4_mbr_write and an MBR on a disk with 4096 byte
# sectors (one Linux partition, sectors 256-1279) for ext4_mbr_scan.
rm -f "$1" "$1.4k" "$1.4k-scan"
truncate -s 16M "$1"
truncate -s 16M "$1.4k"
truncate -s 16M "$1.4k-scan"
python3 - "$1.4k-scan" <<'EOF'
import struct, sys
with open(sys.argv[1], "r+b") as f:
    f.seek(446)
    f.write(struct.pack("<B3sB3sII", 0, b"\0\0\0", 0x83, b"\0\0\0", 256, 1024))
    f.seek(510)
    f.write(b"\x55\xaa")
    # Leftovers beyond the first 512 bytes of sector 0
    f.write(b"\xa5" * (4096 - 512))
EOF
