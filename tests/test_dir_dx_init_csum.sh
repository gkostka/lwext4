# ext4 with dir_index and metadata_csum (the mke2fs defaults) and a journal.
lwext4_mke2fs -t ext4 -b 1024 -O metadata_csum,dir_index "$1" 16M
