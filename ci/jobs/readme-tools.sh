# env: acceptance
# SPDX-License-Identifier: BSD-3-Clause
# Every option of lwext4-generic, lwext4-mkfs, lwext4-mbr, lwext4-server and
# lwext4-client; library build claims (C library only dependency, GPLv2
# file list, BSD-only build, lib_only, shared library).
cmake -S . -B build_generic -DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build build_generic -j"$(nproc)"
sh tests/acceptance/test-tools.sh
sh tests/acceptance/test-build.sh
