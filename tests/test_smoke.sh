# Plain ext4 image with a single known file.
dir="$1.d"
mkdir -p "$dir"
printf 'hello lwext4\n' > "$dir/hello.txt"
lwext4_mke2fs -t ext4 -b 4096 -d "$dir" "$1" 8M
