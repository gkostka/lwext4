# Sparse images large enough for a 64 MiB file: 4 KiB block ext4 with a
# journal, and 1 KiB block ext3 (journal, indirect block mapping).
lwext4_mke2fs -t ext4 -b 4096 "$1" 128M
lwext4_mke2fs -t ext3 -b 1024 "$1.ext3" 32M
