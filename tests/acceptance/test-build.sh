#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Build related README.md claims for the host:
#  - "only C standard library dependency" (checked on the LIB_ONLY build),
#  - the licensing statement: exactly the listed files are GPLv2, and a
#    BSD-3-Clause library can be built without them,
#  - the Makefile's lib_only target and the LWEXT4_BUILD_SHARED_LIB option,
#  - "make install" installs working tools (the README "Compile & install
#    tools" block itself runs in readme-native/readme-debian).
set -eu
. "$(dirname "$0")/lib.sh"

cd "$TOP_DIR"
# sort and comm have to agree on the collation
LC_ALL=C
export LC_ALL

# ISO C library functions lwext4 may use; anything else would be a
# dependency the README does not admit.
C_STDLIB="abort assert calloc free malloc realloc memchr memcmp memcpy memmove
memset printf puts putchar fflush qsort strcat strchr strcmp strcpy strlen
strncmp strncpy strnlen strrchr strtoul strtol snprintf sprintf vprintf"

step "make lib_only"
make lib_only >"$WORK/lib_only.log" 2>&1 ||
	{ tail -20 "$WORK/lib_only.log"; die "make lib_only failed"; }
make -C build_lib_only >"$WORK/lib_only_build.log" 2>&1 ||
	{ tail -20 "$WORK/lib_only_build.log"; die "building build_lib_only failed"; }
lib=build_lib_only/src/liblwext4.a
[ -f "$lib" ] || die "make lib_only did not produce $lib"
pass "make lib_only builds $lib"

step "only C standard library dependency"
nm -g --defined-only "$lib" | awk 'NF == 3 { print $3 }' | sort -u >"$WORK/defined.txt"
nm -u "$lib" | awk 'NF { print $NF }' | grep -v ':$' | sort -u >"$WORK/undefined.txt"
comm -23 "$WORK/undefined.txt" "$WORK/defined.txt" >"$WORK/external.txt"
printf '%s\n' $C_STDLIB | sort -u >"$WORK/allowed.txt"
# Compiler runtime helpers (stack protector) are not library dependencies.
grep -v '^__stack_chk_' "$WORK/external.txt" |
	comm -23 - "$WORK/allowed.txt" >"$WORK/unexpected.txt"
if [ -s "$WORK/unexpected.txt" ]; then
	cat "$WORK/unexpected.txt"
	die "liblwext4.a needs symbols outside the C standard library"
fi
pass "liblwext4.a only needs: $(tr '\n' ' ' <"$WORK/external.txt")"

step "licensing: GPLv2 files"
# README.md lists the GPLv2 files as bullet points after "At this point
# there are two files licensed under GPLv2:".
sed -n '/licensed under GPLv2:/,/^$/s/^\* *//p' README.md | sort >"$WORK/readme-gpl.txt"
[ -s "$WORK/readme-gpl.txt" ] || die "could not find the GPLv2 file list in README.md"
# A file carries its license as the license text or as an SPDX identifier.
(cd src && grep -l -e 'GNU General Public License' \
	-e 'SPDX-License-Identifier: GPL-2.0' ./*.c ../include/*.h ../include/misc/*.h || :) |
	sed 's|^\./||; s|^\.\./|../|' | sort >"$WORK/actual-gpl.txt"
diff -u "$WORK/readme-gpl.txt" "$WORK/actual-gpl.txt" ||
	die "README.md's GPLv2 file list does not match the sources"
pass "GPLv2 files are exactly: $(tr '\n' ' ' <"$WORK/actual-gpl.txt")"
for f in src/*.c include/*.h include/misc/*.h; do
	case " $(tr '\n' ' ' <"$WORK/actual-gpl.txt") " in
	*" $(basename "$f") "*) continue ;;
	esac
	grep -q -e 'Redistribution and use in source and binary forms' \
		-e 'SPDX-License-Identifier: BSD-3-Clause' "$f" ||
		die "$f carries no BSD-3-Clause license (text or SPDX identifier)"
done
pass "all other sources and headers carry the BSD-3-Clause license"

step "BSD-3-Clause build without the GPLv2 files"
# "To use library as a BSD3, GPLv2 licensed source files must be removed
# first": remove them, disable the features they implement, and check that
# the library is complete (links into a program using the public API).
bsd="$WORK/bsd-src"
rm -rf "$bsd"
mkdir -p "$bsd"
cp -R CMakeLists.txt src include blockdev toolchain "$bsd/"
while read -r f; do
	rm "$bsd/src/$f"
done <"$WORK/actual-gpl.txt"
cmake -S "$bsd" -B "$WORK/bsd-build" -DLIB_ONLY=TRUE -DLWEXT4_BUILD_TESTS=OFF \
	-DCMAKE_C_FLAGS="-DCONFIG_EXTENTS_ENABLE=0 -DCONFIG_XATTR_ENABLE=0" \
	>"$WORK/bsd.log" 2>&1 || { cat "$WORK/bsd.log"; die "cmake failed"; }
cmake --build "$WORK/bsd-build" >>"$WORK/bsd.log" 2>&1 ||
	{ tail -30 "$WORK/bsd.log"; die "library does not build without the GPLv2 files"; }
cat >"$WORK/bsd-main.c" <<'EOF'
#include <ext4.h>
#include <ext4_mkfs.h>
int main(void)
{
	/* reference the whole public API surface the library offers */
	return ext4_mount("x", "/", false) + ext4_fremove("/x") +
	       ext4_dir_rm("/x") + ext4_journal_start("/") + ext4_recover("/") +
	       ext4_mkfs(0, 0, 0, 0) + ext4_fsymlink("/a", "/b");
}
EOF
cc -I"$bsd/include" -I"$WORK/bsd-build/include" "$WORK/bsd-main.c" \
	"$WORK/bsd-build/src/liblwext4.a" -o "$WORK/bsd-main" >"$WORK/bsd-link.log" 2>&1 ||
	{ cat "$WORK/bsd-link.log"; die "library without the GPLv2 files has unresolved symbols"; }
pass "library builds and links without $(tr '\n' ' ' <"$WORK/actual-gpl.txt")(extents and xattr disabled)"

step "shared library (LWEXT4_BUILD_SHARED_LIB)"
cmake -S . -B "$WORK/shared" -DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DLWEXT4_BUILD_SHARED_LIB=ON -DCMAKE_BUILD_TYPE=Release >"$WORK/shared.log" 2>&1 ||
	{ cat "$WORK/shared.log"; die "cmake failed"; }
cmake --build "$WORK/shared" >>"$WORK/shared.log" 2>&1 ||
	{ tail -30 "$WORK/shared.log"; die "shared library build failed"; }
[ -f "$WORK/shared/src/liblwext4.so" ] || die "no liblwext4.so"
ldd "$WORK/shared/fs_test/lwext4-mkfs" | grep -q 'liblwext4.so' ||
	die "lwext4-mkfs is not linked against liblwext4.so"
img="$WORK/shared.img"
truncate -s 32M "$img"
"$WORK/shared/fs_test/lwext4-mkfs" -i "$img" -e 4 >/dev/null
"$WORK/shared/fs_test/lwext4-generic" -i "$img" -c 2 -d 10 >/dev/null
fsck_clean "$img" "tools linked against liblwext4.so"
pass "LWEXT4_BUILD_SHARED_LIB=ON builds liblwext4.so and working tools"

finish
