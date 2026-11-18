# Disk image with a filesystem in a window that does not start at offset 0:
# a 1 MiB prefix and a 1 MiB suffix filled with a 0xa5 pattern surround an
# 8 MiB ext4 partition built by mke2fs.
dir="$1.d"
mkdir -p "$dir"
printf 'hello lwext4\n' > "$dir/hello.txt"
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1.part" 8M
head -c 1048576 /dev/zero | tr '\000' '\245' > "$1.pad"
cat "$1.pad" "$1.part" "$1.pad" > "$1"
rm -f "$1.pad" "$1.part"

# Filesystem the test copies into an MBR partition written by lwext4.
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1.fs" 16M
