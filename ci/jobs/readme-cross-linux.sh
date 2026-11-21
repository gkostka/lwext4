# env: cross-linux
# SPDX-License-Identifier: BSD-3-Clause
# README.md's big endian example (s390x under qemu-user) word for word, then
# the feature and journal acceptance tests with that s390x build: "little/big
# endian architectures supported".
sh tests/acceptance/readme-blocks.sh run run-regression-tests#2
export BUILD=build_s390x EMU="qemu-s390x -L /usr/s390x-linux-gnu"
sh tests/acceptance/test-features.sh
CRASH_STEP=3 sh tests/acceptance/test-journal.sh
