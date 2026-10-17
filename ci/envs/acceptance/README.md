# acceptance

[native](../native) (host gcc/clang on top of [base](../base), which has
cmake, make, git and e2fsprogs) plus what the README acceptance tests in
[tests/acceptance](../../../tests/acceptance) additionally use:

| Package | Why |
|---|---|
| binutils | `nm`: checks that liblwext4 only needs the C standard library |
| fdisk | `sfdisk` writes the partition tables `lwext4-mbr` has to read |
| psmisc | `killall`, used by the `server_kill` target in `fs_test.mk` |

e2fsprogs (from base) is the independent oracle: `e2fsck -fn` must find
nothing to fix and `debugfs` must read back what lwext4 wrote.

Jobs: `readme-docs`, `readme-native`, `readme-api`, `readme-tools`.
