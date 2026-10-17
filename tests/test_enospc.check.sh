# SPDX-License-Identifier: BSD-3-Clause
for img in "$1" "$1.ext2" "$1.inodes"; do
	check_fsck "$img"
done
check_eq 0 "$(check_debugfs "$1.inodes" stats |
	sed -n 's/^Free inodes: *//p')" "free inodes"
