# SPDX-License-Identifier: BSD-3-Clause
# env: native
# The installed library used by another project (tests/install_package.sh:
# pkg-config, find_package(lwext4 CONFIG) and plain -I/-L), with the same
# mechanical red/green check red-green.sh does for tests/test_*.c:
#
#   RED:   the base's sources must fail this branch's install_package
#          test (tests/install_package.sh) because files are missing, and
#   GREEN: this branch must pass it.
#
#   ci/run.sh install-package [base ref]
#
# The base defaults to the merge base with fix/issue-48-install-prefix
# (gkostka/lwext4#119, which this work builds on), looked up as
# origin/, fork/ or a local branch.
. ci/scripts/common.sh

base_ref=${1:-}
if [ -z "$base_ref" ]; then
	for r in origin/fix/issue-48-install-prefix \
		 fork/fix/issue-48-install-prefix fix/issue-48-install-prefix; do
		if git rev-parse -q --verify "$r^{commit}" >/dev/null; then
			base_ref=$r
			break
		fi
	done
fi
[ -n "$base_ref" ] ||
	{ echo "no base ref (fetch fix/issue-48-install-prefix)" >&2; exit 2; }
base=$(git merge-base HEAD "$base_ref")
echo "base: $base_ref, merge base $(git log -1 --format='%h %s' "$base")"

work=$(ci_build_dir install-package)
mkdir -p "$work/bin"
ln -sf "$(command -v gcc)" "$work/bin/cc"
export PATH="$PWD/$work/bin:$PATH"

build()
{
	src=$1
	dir=$2
	shift 2
	cmake -S "$src" -B "$dir" -DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
		-DCMAKE_BUILD_TYPE=Release "$@" >"$dir.log" 2>&1 &&
		cmake --build "$dir" -j"$(ci_jobs)" >>"$dir.log" 2>&1 ||
		{ tail -40 "$dir.log"; exit 1; }
}

status=0

# RED: the base's sources, built without their tests (on a merge of other
# branches, e.g. a pull request ref, the tree's other tests may need APIs
# the base lacks), and the branch's install_package test run directly.
mkdir -p "$work/red-src"
git archive "$base" | tar -x -C "$work/red-src"
git archive HEAD tests/install_package.sh tests/package |
	tar -x -C "$work/red-src"
build "$work/red-src" "$work/red" -DLWEXT4_BUILD_TESTS=OFF
if sh "$work/red-src/tests/install_package.sh" cmake "$PWD/$work/red" \
    "$PWD/$work/red-work" "$PWD/$work/red-src" "$(command -v cc)" "" "" \
    >"$work/red.test.log" 2>&1; then
	echo "FAIL  install_package passes against the base"
	status=1
elif grep -q '^missing: ' "$work/red.test.log"; then
	echo "RED   install_package fails against the base:"
	grep -E '^(missing|FAIL): ' "$work/red.test.log" | sed 's/^/        /'
else
	echo "FAIL  install_package fails against the base, but not because"
	echo "      of missing files:"
	tail -40 "$work/red.test.log" | sed 's/^/        /'
	status=1
fi

# GREEN: the branch head.
build . "$work/green"
if ctest --test-dir "$work/green" -R '^install_package$' -V \
    >"$work/green.test.log" 2>&1; then
	echo "GREEN install_package passes on the branch:"
	grep -E '^[0-9]+: (ok|pkg-config|cmake): ' "$work/green.test.log" |
		sed 's/^[0-9]*: /        /'
else
	cat "$work/green.test.log"
	echo "FAIL  install_package fails on the branch"
	status=1
fi
exit $status
