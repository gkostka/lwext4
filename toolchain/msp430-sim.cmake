# SPDX-License-Identifier: BSD-3-Clause
# Name of the target
set(CMAKE_SYSTEM_NAME Generic)
# MSP430X large memory model for the GDB simulator (msp430-elf-run), which
# runs the tests; the -msim link option is added by tests/baremetal.
set(CMAKE_SYSTEM_PROCESSOR msp430-sim)

set(MCPU_FLAGS "-mcpu=msp430x -mlarge -mcode-region=either -mdata-region=either")

include(${CMAKE_CURRENT_LIST_DIR}/common/msp430-gcc.cmake)
