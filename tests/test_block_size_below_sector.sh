# ext4 with 1 KiB blocks; the test presents it as a 4 KiB sector device.
lwext4_mke2fs -t ext4 -b 1024 "$1" 16M
