# Issue #90: s_log_block_size values for which 1024 << s_log_block_size is
# not a valid block size (above 64 KiB, or overflowing 32 bits to 0).
for log in 7 22 30; do
	lwext4_mke2fs -t ext4 -b 4096 "$1.$log" 8M
	debugfs -w -R "ssv log_block_size $log" "$1.$log" 2>/dev/null
done
