# SPDX-License-Identifier: BSD-3-Clause
img="$1"
check_fsck "$img"

# Link counts and inode numbers as e2fsprogs sees them.
check_eq 2 "$(check_stat_field "$img" /a/file Links)" "/a/file links"
check_eq "$(check_debugfs "$img" 'ls -l /a' | awk '$NF == "file" { print $1 }')" \
	"$(check_debugfs "$img" 'ls -l /b' | awk '$NF == "link2" { print $1 }')" \
	"/a/file and /b/link2 inode"
check_eq 'hard link data' "$(check_debugfs "$img" 'cat /b/link2')" "/b/link2"
check_eq 'rename me' "$(check_debugfs "$img" 'cat /b/r3')" "/b/r3"
check_eq inner "$(check_debugfs "$img" 'cat /b/moved/inner')" "/b/moved/inner"
check_eq 'made by mke2fs' "$(check_debugfs "$img" 'cat /b/child/f')" \
	"/b/child/f"
check_eq 4 "$(check_stat_field "$img" /b Links)" "/b links"
check_eq 2 "$(check_stat_field "$img" /a Links)" "/a links"
for gone in /a/r1 /a/r2 /a/sub /a/link1 /tree /pre/child; do
	check_debugfs "$img" "stat $gone" | grep -q . &&
		check_fail "$gone still exists"
done

# Symlinks, special files, attributes.
check_eq 'Fast link dest: "a/file"' \
	"$(check_debugfs "$img" 'stat /fast' | grep 'Fast link dest')" "/fast"
check_eq 'Fast link dest: "now/fast"' \
	"$(check_debugfs "$img" 'stat /was_slow' | grep 'Fast link dest')" \
	"/was_slow"
check_eq 599 "$(check_stat_field "$img" /slow Size)" "/slow size"
check_eq 'Type: character' \
	"$(check_debugfs "$img" 'stat /chr' | grep -o 'Type: character')" "/chr"
check_eq 'Device major/minor number: 01:03' \
	"$(check_debugfs "$img" 'stat /chr' | grep -o 'Device major/minor number: [0-9:]*')" \
	"/chr device"
check_eq 'Type: block' \
	"$(check_debugfs "$img" 'stat /blk' | grep -o 'Type: block')" "/blk"
check_eq 'Type: FIFO' \
	"$(check_debugfs "$img" 'stat /fifo' | grep -o 'Type: FIFO')" "/fifo"
check_eq 'Type: socket' \
	"$(check_debugfs "$img" 'stat /sock' | grep -o 'Type: socket')" "/sock"
check_eq 0640 "$(check_stat_field "$img" /a/file Mode)" "/a/file mode"
check_eq 1234 "$(check_stat_field "$img" /a/file User)" "/a/file uid"
check_eq 5678 "$(check_stat_field "$img" /a/file Group)" "/a/file gid"
check_eq '01234567AB!' "$(check_debugfs "$img" 'cat /seek')" "/seek"
