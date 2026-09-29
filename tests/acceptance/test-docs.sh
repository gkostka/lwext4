#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Consistency of the documentation with the tests and the repository:
#  - every README.md code block is covered by the inventory in
#    tests/acceptance/README.md (and the inventory has no stale entries),
#  - the Debian dependency commands in README.md are the ones the
#    acceptance-debian container installs,
#  - the files and directories README.md's "Project tree" lists exist,
#  - every Makefile target README.md mentions exists.
set -eu
. "$(dirname "$0")/lib.sh"

step "README.md code blocks vs. inventory"
sh "$ACC_DIR/readme-blocks.sh" check
pass "every README.md code block is in the inventory"

step "README.md Debian dependencies vs. ci/envs/acceptance-debian"
dockerfile="$TOP_DIR/ci/envs/acceptance-debian/Dockerfile"
readme_deps=$(sh "$ACC_DIR/readme-blocks.sh" show dependencies#2)
docker_deps=$(sed -n 's/^RUN \(apt-get install\) -y /\1 /p' "$dockerfile" | head -n 1)
[ "$readme_deps" = "$docker_deps" ] ||
	die "README: '$readme_deps', Dockerfile: '$docker_deps'"
pass "README.md Debian package list is what acceptance-debian installs"
tr '\n' ' ' <"$TOP_DIR/README.md" | grep -q 'need `mke2fs` (e2fsprogs)' ||
	die "README.md no longer says the regression tests need e2fsprogs"
grep -q '^RUN apt-get install -y e2fsprogs$' "$dockerfile" ||
	die "acceptance-debian does not install e2fsprogs"
pass "README.md e2fsprogs requirement is what acceptance-debian installs"

step "README.md project tree"
sed -n '/^Project tree/,/^Compile/p' "$TOP_DIR/README.md" |
	sed -n 's/^\* *\([^ ]*\) .*/\1/p' >"$WORK/tree.txt"
[ -s "$WORK/tree.txt" ] || die "could not parse the project tree"
while read -r entry; do
	[ -e "$TOP_DIR/$entry" ] || die "README.md lists '$entry', which does not exist"
	pass "project tree entry exists: $entry"
done <"$WORK/tree.txt"

step "Makefile targets named in README.md"
# The make database lists every target; nothing is run.
make -C "$TOP_DIR" -pRrq : 2>/dev/null |
	sed -n 's/^\([a-zA-Z0-9_+.-][^:=# \t]*\):.*/\1/p' | sort -u >"$WORK/targets.txt"
# "make <target>" in the prose, and in code blocks before any "cd" (after
# "cd build_*" it is the CMake generated Makefile's target)
{
	grep -o '`make [a-z0-9_+-]*`' "$TOP_DIR/README.md" | tr -d '`'
	for key in $(sh "$ACC_DIR/readme-blocks.sh" list | cut -f1); do
		sh "$ACC_DIR/readme-blocks.sh" show "$key" |
			awk '/^cd / { exit } /^make [a-z0-9_+-]*$/ { print }'
	done
} | awk '{ print $2 }' | sort -u >"$WORK/readme-targets.txt"
for target in $(cat "$WORK/readme-targets.txt"); do
	grep -Fqx -- "$target" "$WORK/targets.txt" ||
		die "README.md mentions 'make $target', which the Makefile does not have"
	pass "Makefile has target $target"
done

step "ci/run.sh jobs named in README.md"
# README.md shows "ci/run.sh <job> [arg]"; the job has to exist and be one
# of the jobs .github/workflows/ci.yml runs (with that matrix value).
for key in $(sh "$ACC_DIR/readme-blocks.sh" list | cut -f1); do
	sh "$ACC_DIR/readme-blocks.sh" show "$key"
done | sed -n 's/^ci\/run\.sh \([a-z0-9][a-z0-9_-]*\) *\([a-z0-9_-]*\)$/\1 \2/p' >"$WORK/jobs.txt"
[ -s "$WORK/jobs.txt" ] || die "no ci/run.sh jobs found in README.md"
ci_yml="$TOP_DIR/.github/workflows/ci.yml"
while read -r job arg; do
	[ -f "$TOP_DIR/ci/jobs/$job.sh" ] ||
		die "README.md runs 'ci/run.sh $job', there is no ci/jobs/$job.sh"
	grep -Eq "ci/run.sh $job( |\$)|job: \[.*\b$job\b" "$ci_yml" ||
		die "ci.yml does not run the job '$job' README.md shows"
	[ -z "$arg" ] || grep -Eq "^ *(- |(arch|toolchain): )$arg\$" "$ci_yml" ||
		die "ci.yml does not run '$job $arg'"
	pass "README.md's 'ci/run.sh $job $arg' is a job ci.yml runs"
done <"$WORK/jobs.txt"

finish
