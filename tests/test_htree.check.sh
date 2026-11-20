# SPDX-License-Identifier: BSD-3-Clause
# e2fsck recomputes the hash of every entry and checks the tree.
for img in "$1" "$1.tea" "$1.legacy"; do
	check_fsck "$img"
done
check_eq 1 "$(check_debugfs "$1" 'htree_dump /big' |
	sed -n 's/.*Indirect levels: *//p')" "index levels of /big"
check_eq 'Hash Version: 2' "$(check_debugfs "$1.tea" 'htree_dump /big' |
	grep -o 'Hash Version: [0-9]*')" "tea hash version"
check_eq 'Hash Version: 0' "$(check_debugfs "$1.legacy" 'htree_dump /big' |
	grep -o 'Hash Version: [0-9]*')" "legacy hash version"
