# SPDX-License-Identifier: BSD-3-Clause
# ext4 with 1 KiB blocks (small leaves and index nodes) and each directory
# hash algorithm: half_md4 (the default), tea and legacy.
lwext4_mke2fs -t ext4 -O ^metadata_csum -b 1024 -N 4096 "$1" 16M
lwext4_mke2fs -t ext4 -O ^metadata_csum -b 1024 "$1.tea" 8M
tune2fs -E hash_alg=tea "$1.tea" >/dev/null
lwext4_mke2fs -t ext4 -O ^metadata_csum -b 1024 "$1.legacy" 8M
tune2fs -E hash_alg=legacy "$1.legacy" >/dev/null
