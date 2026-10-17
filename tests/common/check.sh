# SPDX-License-Identifier: BSD-3-Clause
# Sourced by run_test.sh before a test's check script (test_<name>.check.sh),
# which runs on the host after the test executable succeeded. The helpers
# verify the image with e2fsprogs, independently of lwext4. Every failed
# check exits the check script with status 1.

# check_fail <message>
check_fail()
{
	echo "check: $*" >&2
	exit 1
}

# check_fsck <image>: e2fsck -fn must find nothing to fix.
check_fsck()
{
	if ! out=$(e2fsck -fn "$1" 2>&1); then
		printf '%s\n' "$out" | head -60 >&2
		check_fail "e2fsck -fn $1 reports errors"
	fi
}

# check_debugfs <image> <request>: print what debugfs says (stderr, where
# debugfs reports its own banner and errors, is dropped).
check_debugfs()
{
	debugfs -R "$2" "$1" 2>/dev/null
}

# check_eq <expected> <actual> <what>
check_eq()
{
	[ "$1" = "$2" ] || check_fail "$3: expected '$1', got '$2'"
}

# check_stat_field <image> <path> <field regex>: value after "<field>:" in
# "debugfs stat", e.g. check_stat_field img /f Links.
check_stat_field()
{
	check_debugfs "$1" "stat \"$2\"" |
		sed -n "s/.*$3: *\([^ ]*\).*/\1/p" | head -n 1
}
