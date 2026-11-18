# Sourced by run_test.sh before a test's setup script.

# mke2fs wrapper that switches off features newer e2fsprogs enable by default
# but lwext4 does not support. Older e2fsprogs do not know these feature
# names, so fall back to a plain mke2fs call there.
lwext4_mke2fs()
{
	mke2fs -q -F -O ^orphan_file,^metadata_csum_seed "$@" 2>/dev/null ||
		mke2fs -q -F "$@"
}
