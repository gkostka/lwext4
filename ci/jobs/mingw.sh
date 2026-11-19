# SPDX-License-Identifier: BSD-3-Clause
# env: mingw
# platform: linux/amd64
# Windows x86_64 (LLP64: 32-bit long, MSVCRT/UCRT stdio): cross compile with
# MinGW-w64 and run the CTest suite and the e2fsprogs round trip with the
# Windows executables under Wine. They use the portable stdio file block
# device (blockdev/linux/file_dev.c) on image files; the raw
# \\.\PhysicalDrive device (blockdev/windows) needs a real Windows disk.
. ci/scripts/common.sh
x86_64-w64-mingw32-gcc --version | head -n 1
wine --version

# Create the Wine prefix up front, so its setup noise and time are not
# attributed to the first test.
# Wine refuses to create its prefix in a directory the user does not own.
export WINEPREFIX="$HOME/wine-prefix"
mkdir -p "$WINEPREFIX"
wineboot -i >/dev/null 2>&1 || echo "wineboot exited with $?"

build=$(ci_build_dir mingw)
cmake -S . -B "$build" -DCMAKE_TOOLCHAIN_FILE=toolchain/mingw.cmake \
	-DCMAKE_BUILD_TYPE=Release -DCMAKE_CROSSCOMPILING_EMULATOR=wine
cmake --build "$build" -j"$(ci_jobs)"
file "$build/fs_test/lwext4-generic.exe"
wine "$build/fs_test/lwext4-generic.exe" -x

# Wine has to run our code and pass its exit status on: without an image
# argument a test prints its usage and exits with 2.
status=0
wine "$build/tests/test_smoke.exe" || status=$?
[ "$status" -eq 2 ] || { echo "wine exit status $status, expected 2" >&2; exit 1; }

ctest --test-dir "$build" -V
ci/scripts/fs-roundtrip.sh "$build" wine
tail -n 4 "$build/roundtrip/mke2fs-ext4-4096.generic.log"
