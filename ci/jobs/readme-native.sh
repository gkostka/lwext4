# env: acceptance
# SPDX-License-Identifier: BSD-3-Clause
# README.md's commands in README order on the host (build, install, make
# test, the lwext4-generic and lwext4-mkfs examples, make test_all, ctest),
# every result checked with e2fsck/debugfs.
sh tests/acceptance/test-readme-native.sh
