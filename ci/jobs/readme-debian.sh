# env: acceptance-debian
# SPDX-License-Identifier: BSD-3-Clause
# README.md's build, install and test commands on a Debian that has nothing
# but the packages README.md lists (ci/envs/acceptance-debian).
export README_DESTDIR="$PWD/build_acceptance/debian-install"
blocks="sh tests/acceptance/readme-blocks.sh run"
$blocks compile-install-tools#1
$blocks lwext4-generic-demo-application#2
$blocks using-lwext4-mkfs-tool#1 using-lwext4-mkfs-tool#4
e2fsck -fn ext_image
rm -f ext_image
$blocks run-regression-tests#1
