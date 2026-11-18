# Valid ext2 and ext4 images with 1, 2 and 4 KiB blocks (1 KiB blocks have
# s_first_data_block = 1) spanning several block groups.
for bs in 1024 2048 4096; do
	dir="$1.$bs.d"
	mkdir -p "$dir"
	printf 'hello lwext4\n' > "$dir/hello.txt"
	lwext4_mke2fs -t ext4 -b $bs -d "$dir" "$1.$bs" 32M
	lwext4_mke2fs -t ext2 -b $bs -d "$dir" "$1.ext2.$bs" 16M
done
