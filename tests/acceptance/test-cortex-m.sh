#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# README.md Cortex-M claims, after the "Build cortex-m0/m3/m4 library"
# blocks have run (ci/jobs/readme-arm-none-eabi.sh):
#  - each build produced an ARM Thumb liblwext4.a for its CPU,
#  - "Memory footprint" .text sizes for cortex-m4 (ext2 only / full ext4),
#  - "The Cortex-M toolchains also build tests/baremetal ... ctest runs it
#    on a QEMU MPS2 board".
set -eu
. "$(dirname "$0")/lib.sh"

cd "$TOP_DIR"

step "Cortex-M libraries"
for cpu in cortex-m0 cortex-m3 cortex-m4; do
	lib=build_$cpu/src/liblwext4.a
	[ -f "$lib" ] || die "$lib was not built"
	arm-none-eabi-objdump -f "$lib" | grep -q 'file format elf32-littlearm' ||
		die "$lib is not an ARM library"
	arch=$(arm-none-eabi-readelf -A "$lib" | sed -n 's/.*Tag_CPU_arch: *//p' | sort -u | tr '\n' ' ')
	pass "$lib: ARM library ($arch)"
done

# .text of an archive: sum of the text column of "size" (text includes
# read-only data, as in the README's numbers)
text_kib()
{
	arm-none-eabi-size "$@" | awk 'NR > 1 { t += $1 } END { printf "%d\n", (t + 512) / 1024 }'
}

# Compile the library for cortex-m4 like build_cortex-m4 does, with extra
# configuration defines, into $WORK/<name>.
build_config()
{
	cfg=$1
	shift
	mkdir -p "$WORK/$cfg"
	for src in src/*.c; do
		arm-none-eabi-gcc -mthumb -mcpu=cortex-m4 -Wall -fno-builtin \
			-std=gnu11 -fdata-sections -ffunction-sections -O2 \
			-Iinclude -Ibuild_cortex-m4/include \
			-DCONFIG_USE_DEFAULT_CONFIG=0 "$@" \
			-c "$src" -o "$WORK/$cfg/$(basename "$src" .c).o" ||
			die "cannot build $src ($cfg)"
	done
}

step "Memory footprint (README: cortex-m4 .text)"
default_kib=$(text_kib build_cortex-m4/src/liblwext4.a)
log "make cortex-m4 (default configuration, debug output on): ${default_kib} KiB"
build_config full -DCONFIG_DEBUG_PRINTF=0 -DCONFIG_DEBUG_ASSERT=0
full_kib=$(text_kib "$WORK"/full/*.o)
build_config ext2 -DCONFIG_DEBUG_PRINTF=0 -DCONFIG_DEBUG_ASSERT=0 \
	-DCONFIG_EXT_FEATURE_SET_LVL=F_SET_EXT2 -DCONFIG_JOURNALING_ENABLE=0 \
	-DCONFIG_EXTENTS_ENABLE=0 -DCONFIG_XATTR_ENABLE=0
ext2_kib=$(text_kib "$WORK"/ext2/*.o)
log "full ext4 feature set, no debug output: ${full_kib} KiB"
log "ext2 only (no journal, extents, xattr), no debug output: ${ext2_kib} KiB"

within()
{
	[ $(($1 * 100)) -ge $(($2 * 75)) ] && [ $(($1 * 100)) -le $(($2 * 125)) ]
}

step "RAM footprint on QEMU mps2-an386 (README: RAM and .stack)"
# footprint.c measures peak heap and stack of an ext2 (8 block cache, no
# journal/extents build) and an ext4 (journal, extents) workload.
ram()
{
	name=$1
	ext=$2
	shift 2
	build_config "ram-$name" -DCONFIG_DEBUG_PRINTF=0 -DCONFIG_DEBUG_ASSERT=0 \
		-DCONFIG_BLOCK_DEV_CACHE_SIZE=8 "$@"
	arm-none-eabi-gcc -mthumb -mcpu=cortex-m4 -O2 -std=gnu11 -Iinclude \
		-Ibuild_cortex-m4/include -DCONFIG_USE_DEFAULT_CONFIG=0 \
		-DCONFIG_BLOCK_DEV_CACHE_SIZE=8 -DFOOTPRINT_EXT="$ext" "$@" \
		"$ACC_DIR/footprint.c" tests/baremetal/startup.c \
		"$WORK/ram-$name"/*.o --specs=rdimon.specs \
		-T tests/baremetal/mps2.ld -Wl,--gc-sections \
		-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free \
		-o "$WORK/footprint-$name.elf" ||
		die "cannot link the footprint firmware ($name)"
	timeout 300 qemu-system-arm -M mps2-an386 -nographic -monitor none \
		-serial none -semihosting-config enable=on,target=native \
		-kernel "$WORK/footprint-$name.elf" >"$WORK/footprint-$name.txt" 2>&1 ||
		{ cat "$WORK/footprint-$name.txt"; die "footprint firmware ($name) failed"; }
	cat "$WORK/footprint-$name.txt"
	heap=$(sed -n 's/^footprint: heap \([0-9]*\) stack \([0-9]*\)$/\1/p' "$WORK/footprint-$name.txt")
	stack=$(sed -n 's/^footprint: heap \([0-9]*\) stack \([0-9]*\)$/\2/p' "$WORK/footprint-$name.txt")
	static=$(arm-none-eabi-size "$WORK/ram-$name"/*.o |
		awk 'NR > 1 { d += $2 + $3 } END { print d }')
	ram_kib=$(((heap + static + 512) / 1024))
	[ -n "$heap" ] && [ -n "$stack" ] || die "no footprint output ($name)"
	log "$name: heap peak $heap B + static data $static B = ${ram_kib} KiB, stack $stack B"
}
ram ext2 2 -DCONFIG_EXT_FEATURE_SET_LVL=F_SET_EXT2 -DCONFIG_JOURNALING_ENABLE=0 \
	-DCONFIG_EXTENTS_ENABLE=0 -DCONFIG_XATTR_ENABLE=0
ext2_ram=$ram_kib
ext2_stack=$stack
ram ext4 4
ext4_ram=$ram_kib
ext4_stack=$stack

readme_ram_min=$(sed -n 's/^\* RAM: *\([0-9]*\)KB - minimum.*/\1/p' README.md)
readme_ram_full=$(sed -n 's/^\* RAM: .*, *\([0-9]*\)KB - when journaling.*/\1/p' README.md)
readme_stack=$(sed -n 's/^\* \.stack: *\([0-9]*\)KB.*/\1/p' README.md)
[ -n "$readme_ram_min" ] && [ -n "$readme_ram_full" ] && [ -n "$readme_stack" ] ||
	die "cannot find the RAM sizes in README.md"
within "$ext2_ram" "$readme_ram_min" ||
	die "ext2 RAM is ${ext2_ram} KiB, README.md says ${readme_ram_min}KB"
pass "ext2 RAM ${ext2_ram} KiB matches README.md (${readme_ram_min}KB)"
within "$ext4_ram" "$readme_ram_full" ||
	die "journal+extents RAM is ${ext4_ram} KiB, README.md says ${readme_ram_full}KB"
pass "journal+extents RAM ${ext4_ram} KiB matches README.md (${readme_ram_full}KB)"
max_stack=$ext2_stack
[ "$ext4_stack" -gt "$max_stack" ] && max_stack=$ext4_stack
[ "$max_stack" -le $((readme_stack * 1024)) ] ||
	die "stack use is $max_stack bytes, README.md says ${readme_stack}KB is enough"
pass "stack use ($ext2_stack / $ext4_stack bytes) within README.md's ${readme_stack}KB"

step "Memory footprint vs. README.md"
# README.md states the numbers in its "Memory footprint" section; they have
# to be within 25% of what is measured.
readme_ext2=$(sed -n 's/^\* \.text: *\([0-9]*\)KB - only ext2 fs support.*/\1/p' README.md)
readme_full=$(sed -n 's/^\* \.text: .*, *\([0-9]*\)KB - full ext4 fs feature set.*/\1/p' README.md)
[ -n "$readme_ext2" ] && [ -n "$readme_full" ] ||
	die "cannot find the .text sizes in README.md"
within "$ext2_kib" "$readme_ext2" ||
	die "ext2 only .text is ${ext2_kib} KiB, README.md says ${readme_ext2}KB"
pass "ext2 only .text ${ext2_kib} KiB matches README.md (${readme_ext2}KB)"
within "$full_kib" "$readme_full" ||
	die "full ext4 .text is ${full_kib} KiB, README.md says ${readme_full}KB"
pass "full ext4 .text ${full_kib} KiB matches README.md (${readme_full}KB)"

step "tests/baremetal on QEMU MPS2 (README: Run regression tests)"
make -C build_cortex-m3 >"$WORK/m3-build.log" 2>&1 ||
	{ tail -30 "$WORK/m3-build.log"; die "building build_cortex-m3 failed"; }
[ -f build_cortex-m3/tests/baremetal/baremetal_test.elf ] ||
	die "make cortex-m3 did not build tests/baremetal"
(cd build_cortex-m3 && ctest --output-on-failure) >"$WORK/m3-ctest.log" 2>&1 ||
	{ cat "$WORK/m3-ctest.log"; die "baremetal test failed on QEMU"; }
pass "make cortex-m3 builds tests/baremetal and ctest runs it on QEMU MPS2"

finish
