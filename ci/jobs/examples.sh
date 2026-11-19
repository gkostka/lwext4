# SPDX-License-Identifier: BSD-3-Clause
# env: native
# Build and run every example in examples/ (with AddressSanitizer and
# UBSan), check the images they write with e2fsck -fn and debugfs, and
# compile the C snippets of the READMEs and check their links, so the
# getting-started documentation cannot rot.
#
#   ci/run.sh examples
. ci/scripts/common.sh
build=$(ci_build_dir examples)

mkdir -p "$build/bin"
ln -sf "$(command -v gcc)" "$build/bin/cc"
export PATH="$PWD/$build/bin:$PATH"

export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export LSAN_OPTIONS=suppressions=$PWD/ci/lsan.supp
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

cmake -S . -B "$build/b" \
	-DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Release \
	-DLWEXT4_SANITIZE=address,undefined
cmake --build "$build/b" -j"$(ci_jobs)" \
	--target lwext4-example-basic lwext4-example-blockdev-template

ci/scripts/check-examples.sh "$build/b"
python3 ci/scripts/check-docs.py cc "$build/b" README.md examples/README.md
