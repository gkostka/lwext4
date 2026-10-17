# ext4 image with a journal and two files of the same size, so that removing
# either of them produces journal transactions of the same shape.
dir="$1.d"
mkdir -p "$dir"
printf 'victim\n' > "$dir/victim"
printf 'other!\n' > "$dir/other"
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 16M
