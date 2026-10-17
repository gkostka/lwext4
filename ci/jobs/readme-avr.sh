# env: avr
# SPDX-License-Identifier: BSD-3-Clause
# README.md "Build avrxmega7 library" word for word.
sh tests/acceptance/readme-blocks.sh run build-avrxmega7-library#1
avr-objdump -f build_avrxmega7/src/liblwext4.a | grep -q 'file format elf32-avr'
avr-size -t build_avrxmega7/src/liblwext4.a | tail -n 1
echo "README block build-avrxmega7-library#1: AVR library built"
