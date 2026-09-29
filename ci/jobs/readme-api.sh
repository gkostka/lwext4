# env: acceptance
# SPDX-License-Identifier: BSD-3-Clause
# README feature claims through the public API: file types, links, xattrs,
# block sizes, ext2/3/4 feature sets, htree and extents, the unsupported
# features, e2fsprogs interoperability in both directions, journal replay
# and power loss at every block write, cache modes.
cmake -S . -B build_generic -DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Release
cmake --build build_generic -j"$(nproc)"
sh tests/acceptance/test-features.sh
sh tests/acceptance/test-journal.sh
