# SPDX-License-Identifier: BSD-3-Clause
# The cleanly unmounted image and both replayed power cut copies.
for img in "$1" "$1.crash1" "$1.crash2"; do
	check_fsck "$img"
done
