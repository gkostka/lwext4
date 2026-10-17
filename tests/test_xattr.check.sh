# SPDX-License-Identifier: BSD-3-Clause
# What lwext4 wrote, read back by debugfs; both images must pass e2fsck.
for img in "$1" "$1.ext2"; do
	check_fsck "$img"
	check_eq 'user.replace (14) = "replaced value"' \
		"$(check_debugfs "$img" 'ea_get /small user.replace' | head -n 1)" \
		"$img: /small user.replace"
	check_eq 3 "$(check_debugfs "$img" 'ea_list /small' | grep -c '^  ')" \
		"$img: attributes of /small"
	check_eq 'user.nothing (0)' \
		"$(check_debugfs "$img" 'ea_list /empty' | sed -n 's/^  //p')" \
		"$img: /empty"
	check_eq opqrstuvwxyzabcdefghijkl \
		"$(check_debugfs "$img" 'ea_get -V /many user.many-14' |
		cut -c1-24)" "$img: /many user.many-14"
	check_eq after "$(check_debugfs "$img" 'ea_get -V /large user.after-1')" \
		"$img: /large user.after-1"
	check_eq D "$(check_debugfs "$img" 'ea_get -V /dir user.on-dir1')" \
		"$img: /dir user.on-dir1"
done
check_eq 17 "$(check_debugfs "$1" 'ea_list /many' | grep -c '^  ')" \
	"attributes of /many"
check_eq 9 "$(check_debugfs "$1.ext2" 'ea_list /many' | grep -c '^  ')" \
	"ext2: attributes of /many"
check_eq 0 "$(check_debugfs "$1.ext2" 'ea_list /many' |
	grep -c '^  user.many-13')" "ext2: removed /many user.many-13"
