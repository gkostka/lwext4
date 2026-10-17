# Plain ext4 image with a journal. 256 byte inodes and 4 KiB blocks put the
# journal inode and the first regular file inodes in the same block.
lwext4_mke2fs -t ext4 -b 4096 -I 256 "$1" 16M
