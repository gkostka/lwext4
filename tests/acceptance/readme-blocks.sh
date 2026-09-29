#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
#
# Extract and run the shell code blocks of README.md, so the documentation
# itself is what gets tested.
#
#   readme-blocks.sh list          key and first line of every block
#   readme-blocks.sh show <key>    print one block
#   readme-blocks.sh run <key>...  run blocks from the repository root
#   readme-blocks.sh check         every block is accounted for in
#                                  tests/acceptance/README.md, and every key
#                                  mentioned there still exists
#
# A block's key is the slug of the README heading it is under, plus "#n" for
# the nth block under that heading, e.g. "compile-install-tools#1".
#
# "run" executes each block with "bash -ex" in one shell per block (so "cd"
# inside a block works as documented). The only rewrite is for "sudo": CI
# runs unprivileged, so "sudo " is replaced by DESTDIR=<staging directory>
# (honoured by the CMake generated "make install") for that one command, and
# the staging directory's bin directory is put first in PATH. The installed
# tools are therefore what the later blocks run. DESTDIR is not exported to
# the other commands: it would also redirect every other install, e.g. the
# ones the CTest suite makes into its own scratch prefixes.
set -eu

ACC_DIR=$(cd "$(dirname "$0")" && pwd)
TOP_DIR=$(cd "$ACC_DIR/../.." && pwd)
README="$TOP_DIR/README.md"
INVENTORY="$ACC_DIR/README.md"

# Print "key<TAB>line" for every line of every fenced block.
blocks()
{
	awk '
	function slug(s) {
		s = tolower(s)
		gsub(/[^a-z0-9]+/, "-", s)
		gsub(/^-+|-+$/, "", s)
		return s
	}
	{
		line = $0
		sub(/\r$/, "", line)
	}
	!infence && /^[ \t]*```/ {
		infence = 1
		n[heading]++
		key = heading "#" n[heading]
		next
	}
	infence && /^[ \t]*```/ { infence = 0; next }
	infence {
		sub(/^[ \t]+/, "", line)
		if (line != "")
			print key "\t" line
		next
	}
	/^(=+|-+)[ \t]*$/ && prev != "" && prev !~ /^[ \t]*[*-] / {
		heading = slug(prev)
	}
	{ prev = line }
	' "$README"
}

keys()
{
	blocks | cut -f1 | uniq
}

show()
{
	blocks | awk -F '\t' -v k="$1" '$1 == k { print $2; found = 1 }
		END { exit !found }' || {
		echo "readme-blocks: no block '$1' in README.md" >&2
		exit 1
	}
}

run_block()
{
	key=$1
	script=$(show "$key")
	echo "=== README block $key"
	printf '%s\n' "$script" | sed 's/^/    | /'
	# shellcheck disable=SC2016
	script=$(printf '%s\n' "$script" |
		sed "s|^sudo |DESTDIR='$README_DESTDIR' |")
	(
		cd "$TOP_DIR"
		PATH="$README_DESTDIR/usr/local/bin:$PATH:/sbin:/usr/sbin"
		export PATH
		bash -ex -c "$script"
	) || {
		echo "FAIL: README block $key" >&2
		exit 1
	}
	echo "=== README block $key: ok"
}

cmd=${1:-}
[ $# -gt 0 ] && shift
case "$cmd" in
list)
	blocks | awk -F '\t' '$1 != last { print $1 "\t" $2; last = $1 }'
	;;
show)
	[ $# -eq 1 ] || { echo "usage: $0 show <key>" >&2; exit 2; }
	show "$1"
	;;
run)
	[ $# -ge 1 ] || { echo "usage: $0 run <key>..." >&2; exit 2; }
	README_DESTDIR=${README_DESTDIR:-$TOP_DIR/build_readme_install}
	for key in "$@"; do
		run_block "$key"
	done
	;;
check)
	status=0
	for key in $(keys); do
		if grep -Fq "\`readme:$key\`" "$INVENTORY"; then
			echo "ok: $key"
		else
			echo "FAIL: README block $key is not in the inventory" \
			     "(tests/acceptance/README.md)"
			status=1
		fi
	done
	for key in $(grep -o '`readme:[^`]*`' "$INVENTORY" |
		     sed 's/^`readme://; s/`$//' | sort -u); do
		keys | grep -Fqx "$key" || {
			echo "FAIL: inventory mentions readme:$key," \
			     "which is not a README.md block any more"
			status=1
		}
	done
	exit $status
	;;
*)
	sed -n '/^set -eu/q; 4,$s/^# \{0,1\}//p' "$0" >&2
	exit 2
	;;
esac
