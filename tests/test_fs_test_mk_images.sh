# SPDX-License-Identifier: BSD-3-Clause
# "make test" and "make test_all" (fs_test.mk, included by the Makefile)
# create their images with images_small/images_big and check them with
# fsck_images. Run images_small and fsck_images through the Makefile the
# way "make test" does, but
#  - with a sudo that always fails first in PATH: image files need no root,
#  - outside a git checkout (as from a release tarball): the Makefile must
#    not print git errors.
# The test then uses the three images with lwext4.
src=$(cd "$(dirname "$0")/.." && pwd)
work=$(dirname "$1")
mkdir -p "$work/bin"
printf '#!/bin/sh\necho "sudo called: $*" >&2\nexit 1\n' >"$work/bin/sudo"
chmod +x "$work/bin/sudo"
rc=0
(
	cd "$work" &&
	PATH="$work/bin:$PATH" GIT_CEILING_DIRECTORIES=$(dirname "$work") \
		make --no-print-directory -I "$src" -f "$src/Makefile" \
		images_small fsck_images
) >"$work/make.log" 2>"$work/make.err" || rc=$?
cat "$work/make.log" "$work/make.err"
[ $rc -eq 0 ] || { echo "images_small/fsck_images failed ($rc)"; exit 1; }
if grep -i 'git' "$work/make.err"; then
	echo "the Makefile printed git errors"
	exit 1
fi
: >"$1"
