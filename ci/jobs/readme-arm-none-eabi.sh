# env: arm-none-eabi
# SPDX-License-Identifier: BSD-3-Clause
# README.md "Build cortex-m0/m3/m4 library" word for word, the cortex-m4
# memory footprint numbers, and tests/baremetal on QEMU.
sh tests/acceptance/readme-blocks.sh run build-cortex-m0-library#1 \
	build-cortex-m3-library#1 build-cortex-m4-library#1
sh tests/acceptance/test-cortex-m.sh
