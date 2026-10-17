# Issue #39: fast symlinks whose i_blocks is non-zero because they carry an
# extended attribute block, one of them long enough for its target to spill
# into the i_block slots used for indirect block pointers. A plain fast
# symlink and a slow symlink serve as controls.
dir="$1.d"
mkdir -p "$dir"
printf 'hi\n' > "$dir/file.txt"
ln -s file.txt "$dir/fast"
ln -s file.txt "$dir/fast_xattr"
# 59 bytes: the longest target that still fits inline in i_block.
ln -s "$(printf 'b%.0s' $(seq 1 50))/file.txt" "$dir/fast_long_xattr"
ln -s "$(printf 'a%.0s' $(seq 1 100))/file.txt" "$dir/slow"
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 8M

# An xattr value too large for the in-inode xattr area forces a separate
# xattr block, which is accounted for in i_blocks of the symlink inode.
head -c 300 /dev/zero | tr '\0' 'x' > "$1.val"
debugfs -w -R "ea_set -f $1.val fast_xattr user.test" "$1" >/dev/null 2>&1
debugfs -w -R "ea_set -f $1.val fast_long_xattr user.test" "$1" >/dev/null 2>&1
e2fsck -fn "$1" >/dev/null 2>&1
