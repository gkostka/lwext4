# ext4 image with one empty file: removing it touches no data blocks, only
# the inode bitmap.
dir="$1.d"
mkdir -p "$dir"
: > "$dir/empty.txt"
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 8M
