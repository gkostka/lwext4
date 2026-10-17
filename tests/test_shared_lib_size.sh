# SPDX-License-Identifier: BSD-3-Clause
# Build lwext4 as a shared library (LWEXT4_BUILD_SHARED_LIB=ON) with the
# toolchain of this build. The toolchain defines SIZE, so the build runs
# the lib_size target; the build output is the test's input.
build="$1.build"
cmake -G "$LWEXT4_GENERATOR" -C "$LWEXT4_INITIAL_CACHE" \
	-S "$LWEXT4_SOURCE_DIR" -B "$build" -DLIB_ONLY=TRUE \
	-DLWEXT4_BUILD_TESTS=OFF -DLWEXT4_BUILD_SHARED_LIB=ON \
	>"$build.log" 2>&1 || { cat "$build.log"; exit 1; }
cmake --build "$build" >"$1" 2>&1 || { cat "$1"; exit 1; }
