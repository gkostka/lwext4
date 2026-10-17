# SPDX-License-Identifier: BSD-3-Clause
# Name of the target
set(CMAKE_SYSTEM_NAME Generic)
# ATmega1284 (16 KiB RAM, 128 KiB flash): the AVR the tests run on in
# simavr, which has no XMEGA support.
set(CMAKE_SYSTEM_PROCESSOR atmega1284)

set(MCPU_FLAGS "-mmcu=atmega1284 -mcall-prologues")

include(${CMAKE_CURRENT_LIST_DIR}/common/avr-gcc.cmake)
