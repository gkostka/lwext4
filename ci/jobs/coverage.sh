# SPDX-License-Identifier: BSD-3-Clause
# env: coverage
# Line and branch coverage of the library (src/) reached by the CTest suite
# and the e2fsprogs round trip. Prints a per-file table, writes an HTML
# report and fails if the totals drop below ci/coverage-floor.
#
#   ci/run.sh coverage
#
# Output in build-ci/coverage/: report/index.html (annotated sources),
# summary.md (the table), coverage.json (gcovr JSON summary).
. ci/scripts/common.sh
out=$(ci_build_dir coverage)
build=$out/b

# toolchain/generic.cmake hardcodes "cc" and its CMAKE_C_FLAGS; instrument
# through the compiler command instead. Debug = -O0, so lines and branches
# map 1:1 to the source.
mkdir -p "$out/bin"
cat >"$out/bin/cc" <<'EOF'
#!/bin/sh
exec gcc --coverage "$@"
EOF
chmod +x "$out/bin/cc"
export PATH="$PWD/$out/bin:$PATH"
gcc --version | head -n 1
gcovr --version | head -n 1

cmake -S . -B "$build" \
	-DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Debug
cmake --build "$build" -j"$(ci_jobs)"

# Coverage counts only if everything passes: a failing test would make the
# numbers meaningless (and is already a failure of the native jobs).
ctest --test-dir "$build" -j"$(ci_jobs)"
ci/scripts/fs-roundtrip.sh "$build"

# Some tests build the sources a second time with a different configuration
# (e.g. without ext4_xattr.c), where a function can sit on a different line
# (a stub instead of the real one). Count such functions once per
# definition instead of failing on the mismatch; lines and branches of all
# builds are merged as usual.
mkdir -p "$out/report"
gcovr --root . --filter 'src/' "$build" \
	--merge-mode-functions=separate \
	--html-details "$out/report/index.html" \
	--html-title "lwext4 coverage" \
	--json-summary "$out/coverage.json" --json-summary-pretty
python3 ci/scripts/coverage-report.py "$out/coverage.json" \
	ci/coverage-floor "$out/summary.md"
