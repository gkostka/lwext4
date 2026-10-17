# SPDX-License-Identifier: BSD-3-Clause
# env: native
# Mechanical red/green check for a topic branch. Every regression test the
# branch adds (tests/test_*.c that its base does not have) must
#
#   RED:   fail when built from the branch's tests/ against the base's
#          library (a test that does not even build against the base, e.g.
#          because it uses a new API, also counts as red), and
#   GREEN: pass on the branch head.
#
# A test that guards against over-correcting (e.g. valid filesystems must
# still mount) is expected to pass on the base as well; it declares that
# with a "red-green: guard" comment.
#
# Both builds use AddressSanitizer + UBSan, so memory errors count as
# failures.
#
#   ci/run.sh red-green [base ref]
#
# The base defaults to the merge base with the foundation branch
# tests/harness (origin/tests/harness, fork/tests/harness or a local
# tests/harness). A branch that adds no tests passes trivially.
. ci/scripts/common.sh

base_ref=${1:-}
if [ -z "$base_ref" ]; then
	for r in origin/tests/harness fork/tests/harness tests/harness; do
		if git rev-parse -q --verify "$r^{commit}" >/dev/null; then
			base_ref=$r
			break
		fi
	done
fi
[ -n "$base_ref" ] || { echo "no base ref (fetch tests/harness)" >&2; exit 2; }
base=$(git merge-base HEAD "$base_ref")
echo "base: $base_ref, merge base $(git log -1 --format='%h %s' "$base")"

new_tests=$(git diff --name-only --diff-filter=A "$base" HEAD -- 'tests/test_*.c' |
	sed 's|^tests/||; s|\.c$||')
if [ -z "$new_tests" ]; then
	echo "no new tests relative to the base: nothing to check"
	exit 0
fi
echo "new tests:" $new_tests

export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export LSAN_OPTIONS=suppressions=$PWD/ci/lsan.supp
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

work=$(ci_build_dir red-green)
mkdir -p "$work/bin"
ln -sf "$(command -v gcc)" "$work/bin/cc"
export PATH="$PWD/$work/bin:$PATH"

configure()
{
	cmake -S "$1" -B "$2" -DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
		-DCMAKE_BUILD_TYPE=Release -DLWEXT4_SANITIZE=address,undefined \
		>"$2.configure.log" 2>&1 || { cat "$2.configure.log"; exit 1; }
}

# RED: the base's sources with the branch's tests/.
mkdir -p "$work/red-src"
git archive "$base" | tar -x -C "$work/red-src"
rm -rf "$work/red-src/tests"
git archive HEAD tests | tar -x -C "$work/red-src"
configure "$work/red-src" "$work/red"

status=0
for t in $new_tests; do
	if ! cmake --build "$work/red" --target "$t" -j"$(ci_jobs)" \
	    >"$work/red-$t.build.log" 2>&1; then
		echo "RED   $t (does not build against the base)"
		grep -m 3 'error' "$work/red-$t.build.log" | sed 's/^/        /'
		continue
	fi
	cmake --build "$work/red" --target lwext4-mkfs lwext4-generic \
		-j"$(ci_jobs)" >/dev/null 2>&1 || true
	if ctest --test-dir "$work/red" -R "^$t\$" --timeout 300 \
	    >"$work/red-$t.log" 2>&1; then
		if grep -q 'red-green: guard' "tests/$t.c"; then
			echo "GUARD $t passes against the base, as declared"
		else
			echo "FAIL  $t passes against the base: it does not test the fix"
			echo "      (mark tests that must pass on the base too with a"
			echo "      'red-green: guard' comment)"
			status=1
		fi
	elif grep -q 'red-green: guard' "tests/$t.c"; then
		echo "FAIL  $t is a guard test but fails against the base"
		grep -m 3 -E 'failed|ERROR|SUMMARY|Timeout' "$work/red-$t.log" |
			sed 's/^/        /'
		status=1
	else
		echo "RED   $t fails against the base"
		grep -m 3 -E 'failed|ERROR|SUMMARY|Timeout' "$work/red-$t.log" |
			sed 's/^/        /'
	fi
done

# GREEN: the branch head, the whole suite.
configure . "$work/green"
cmake --build "$work/green" -j"$(ci_jobs)" >"$work/green.build.log" 2>&1 ||
	{ tail -40 "$work/green.build.log"; exit 1; }
for t in $new_tests; do
	if ctest --test-dir "$work/green" -R "^$t\$" --timeout 300 \
	    --output-on-failure; then
		echo "GREEN $t passes on the branch"
	else
		echo "FAIL  $t fails on the branch"
		status=1
	fi
done
ctest --test-dir "$work/green" --timeout 300 || status=1
exit $status
