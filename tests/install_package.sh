#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Usage: install_package.sh <cmake> <build dir> <work dir> <source dir>
#                           <cc> <cflags> <ldflags>
#
# Installs the build tree into a scratch prefix, moves the prefix somewhere
# else (the install must be relocatable), checks that every header, library
# and package file is there, then builds tests/package/consumer.c against
# the installed copy in three ways and runs each program:
#
#   pkg-config   cc $(pkg-config --cflags --libs lwext4)
#   cmake        find_package(lwext4 CONFIG REQUIRED), lwext4::blockdev
#   plain        cc -I<prefix>/include/lwext4 -L<prefix>/lib -lblockdev -llwext4
#
# <cflags>/<ldflags> are the flags the library was built with (e.g. the
# sanitizer options), which the consumer needs as well.
set -u

cmake="$1"
build="$2"
work="$3"
src="$4"
cc="$5"
cflags="$6"
ldflags="$7"

rm -rf "$work"
mkdir -p "$work"
status=0

fail()
{
	echo "FAIL: $*"
	status=1
}

# Works with every CMake version (cmake --install needs 3.15).
if ! "$cmake" -DCMAKE_INSTALL_PREFIX="$work/staging" \
    -P "$build/cmake_install.cmake" >"$work/install.log" 2>&1; then
	cat "$work/install.log"
	fail "install"
	exit 1
fi
mv "$work/staging" "$work/prefix"
prefix="$work/prefix"

# Everything a consumer needs.
missing()
{
	echo "missing: $1"
	status=1
}
for h in "$src"/include/*.h "$src"/include/misc/*.h; do
	f=include/lwext4/${h#"$src"/include/}
	[ -f "$prefix/$f" ] || missing "$f"
done
for f in include/lwext4/generated/ext4_config.h \
	 include/lwext4/blockdev/blockdev.h \
	 include/lwext4/blockdev/file_dev.h \
	 lib/libblockdev.a \
	 lib/pkgconfig/lwext4.pc \
	 lib/cmake/lwext4/lwext4Config.cmake \
	 lib/cmake/lwext4/lwext4ConfigVersion.cmake \
	 lib/cmake/lwext4/lwext4Targets.cmake; do
	[ -f "$prefix/$f" ] || missing "$f"
done
# Static or shared, depending on LWEXT4_BUILD_SHARED_LIB
found=
for f in liblwext4.a liblwext4.so liblwext4.dylib; do
	[ -f "$prefix/lib/$f" ] && found=$f
done
[ -n "$found" ] || missing "lib/liblwext4.{a,so,dylib}"

# A shared liblwext4 is found at run time through the library path.
LD_LIBRARY_PATH="$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
DYLD_LIBRARY_PATH="$prefix/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
export LD_LIBRARY_PATH DYLD_LIBRARY_PATH

# run_consumer <way> <executable>
run_consumer()
{
	if "$2" "$work/$1.img" >"$work/$1.run.log" 2>&1; then
		echo "ok: $1"
	else
		cat "$work/$1.run.log"
		fail "$1: the consumer does not run"
	fi
}

# 1. pkg-config, looking only at the scratch prefix.
way=pkg-config
mkdir -p "$work/$way"
PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
export PKG_CONFIG_PATH PKG_CONFIG_LIBDIR
if ! command -v pkg-config >/dev/null 2>&1; then
	fail "$way: pkg-config is not installed"
elif ! pkg_cflags=$(pkg-config --print-errors --cflags lwext4) ||
     ! pkg_libs=$(pkg-config --print-errors --libs lwext4); then
	fail "$way: pkg-config does not find lwext4"
elif ! $cc $cflags $pkg_cflags "$src/tests/package/consumer.c" \
	    $ldflags $pkg_libs -o "$work/$way/consumer" \
	    >"$work/$way.build.log" 2>&1; then
	cat "$work/$way.build.log"
	fail "$way: the consumer does not build"
else
	echo "$way: lwext4 $(pkg-config --modversion lwext4):" \
		$pkg_cflags $pkg_libs
	run_consumer $way "$work/$way/consumer"
fi
unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR

# 2. CMake package.
way=cmake
mkdir -p "$work/$way"
if ! (cd "$work/$way" && "$cmake" "$src/tests/package" \
	    -DCMAKE_PREFIX_PATH="$prefix" \
	    -DCMAKE_C_COMPILER="$cc" \
	    -DCMAKE_C_FLAGS="$cflags" \
	    -DCMAKE_EXE_LINKER_FLAGS="$ldflags" &&
      "$cmake" --build .) >"$work/$way.build.log" 2>&1; then
	cat "$work/$way.build.log"
	fail "$way: the consumer does not configure or build"
elif ! grep -q "^lwext4_DIR:PATH=$prefix/lib/cmake/lwext4\$" \
	    "$work/$way/CMakeCache.txt"; then
	grep '^lwext4_DIR' "$work/$way/CMakeCache.txt"
	fail "$way: find_package did not use the scratch prefix"
else
	echo "$way: find_package(lwext4) from $prefix/lib/cmake/lwext4"
	run_consumer $way "$work/$way/consumer"
fi

# 3. Plain compiler flags.
way=plain
mkdir -p "$work/$way"
if ! $cc $cflags -I"$prefix/include/lwext4" \
	    "$src/tests/package/consumer.c" $ldflags \
	    -L"$prefix/lib" -lblockdev -llwext4 -o "$work/$way/consumer" \
	    >"$work/$way.build.log" 2>&1; then
	cat "$work/$way.build.log"
	fail "$way: the consumer does not build"
else
	run_consumer $way "$work/$way/consumer"
fi

exit $status
