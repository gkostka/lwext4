# SPDX-License-Identifier: BSD-3-Clause
# env: native
# Build for the container's own architecture, run the CTest suite and the
# e2fsprogs round trip.
#
#   ci/run.sh native gcc | clang | asan-ubsan | clang-asan-ubsan
variant=${1:-gcc}
case "$variant" in
gcc)        cc=gcc;   sanitize= ;;
clang)      cc=clang; sanitize= ;;
asan-ubsan) cc=gcc;   sanitize=address,undefined ;;
clang-asan-ubsan) cc=clang; sanitize=address,undefined ;;
*) echo "unknown variant '$variant'" >&2; exit 2 ;;
esac

. ci/scripts/common.sh
build=$(ci_build_dir "native-$variant")

# toolchain/generic.cmake hardcodes "cc"; point it at the requested compiler.
mkdir -p "$build/bin"
ln -sf "$(command -v $cc)" "$build/bin/cc"
export PATH="$PWD/$build/bin:$PATH"
cc --version | head -n 1

export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export LSAN_OPTIONS=suppressions=$PWD/ci/lsan.supp
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake -S . -B "$build/b" \
	-DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Release \
	-DLWEXT4_SANITIZE="$sanitize"
cmake --build "$build/b" -j"$(ci_jobs)"
ctest --test-dir "$build/b"
ci/scripts/fs-roundtrip.sh "$build/b"
