# SPDX-License-Identifier: BSD-3-Clause
check_fsck "$1"
check_fsck "$1.ext2"
check_eq 307200 "$(check_stat_field "$1.ext2" /big Size)" "ext2 /big size"
check_eq 51200 "$(check_stat_field "$1" /a Size)" "ext4 /a size"
check_debugfs "$1" 'stat /b' | grep -q . && check_fail "/b still exists"
# The data as e2fsprogs reads it: block 49 of /a starts with 0x01003100
# (little endian).
check_eq '0031 0001' \
	"$(check_debugfs "$1" 'bd -f /a 49' | sed -n '1s/^0000  \(.........\).*/\1/p')" \
	"ext4 /a block 49"
