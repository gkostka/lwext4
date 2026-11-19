# SPDX-License-Identifier: BSD-3-Clause
# Test firmware for bare-metal and simulated targets (see main.c), run by
# CTest on an emulator or simulator:
#
#   cortex-m*    qemu-system-arm, MPS2 board, ARM semihosting
#   arm-sim      qemu-arm (user mode), newlib rdimon semihosting
#   atmega1284   simavr (8-bit AVR, 16 KiB RAM: no RAM disk)
#   msp430-sim   msp430-elf-run, the GDB MSP430 simulator
#
# The test data (mke2fs images, htree hash reference values) is generated at
# build time by mkimages.py, which needs python3 and e2fsprogs on the host.

# Images to build into the firmware (see mkimages.py), default all.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL atmega1284)
    # 128 KiB of flash: lwext4 leaves room for one image. ext4 covers the
    # most (metadata_csum, extents, 64bit, htree); ext2 (indirect blocks)
    # runs on the other targets.
    set(images ext4)
else()
    set(images "")
endif()

find_program(LWEXT4_PYTHON3 python3)
if(NOT LWEXT4_PYTHON3)
    message(FATAL_ERROR "tests/baremetal needs python3 (or -DLWEXT4_BUILD_TESTS=OFF)")
endif()
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/testdata.h
    COMMAND ${LWEXT4_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/mkimages.py
            ${CMAKE_CURRENT_BINARY_DIR}/images
            ${CMAKE_CURRENT_BINARY_DIR}/testdata.h ${images}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mkimages.py
    COMMENT "Generating test images and hash vectors with e2fsprogs")

set(sources main.c unit.c images.c ${CMAKE_CURRENT_BINARY_DIR}/testdata.h)
set(defines "")

# Startup files are needed here, whatever the library-only toolchain says.
string(REPLACE "-nostartfiles" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^cortex-m")
    set(LWEXT4_QEMU_SYSTEM_ARM qemu-system-arm CACHE STRING
        "qemu-system-arm binary used to run the bare-metal test")
    # ARMv6-M (Cortex-M0/M0+) code also runs on the Cortex-M3 of the AN385.
    if(CMAKE_C_FLAGS MATCHES "-mcpu=cortex-m7")
        set(default_machine mps2-an500)
    elseif(CMAKE_C_FLAGS MATCHES "-mcpu=cortex-m4")
        set(default_machine mps2-an386)
    else()
        set(default_machine mps2-an385)
    endif()
    set(LWEXT4_QEMU_MACHINE ${default_machine} CACHE STRING
        "QEMU machine (-M) for the bare-metal test")
    list(APPEND sources startup.c)
    set(link_flags "--specs=rdimon.specs -T${CMAKE_CURRENT_SOURCE_DIR}/mps2.ld")
    set(runner ${LWEXT4_QEMU_SYSTEM_ARM} -M ${LWEXT4_QEMU_MACHINE}
               -nographic -monitor none -serial none
               -semihosting-config enable=on,target=native -kernel)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL arm-sim)
    set(LWEXT4_QEMU_ARM qemu-arm CACHE STRING
        "qemu-arm (user mode) binary used to run the arm-sim test")
    list(APPEND sources platform_hosted.c)
    set(link_flags "")
    # The firmware is linked at 0x8000, below the usual vm.mmap_min_addr of
    # 64 KiB: give qemu a guest base so the host mapping lands elsewhere.
    set(runner ${LWEXT4_QEMU_ARM} -B 0x40000000)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL atmega1284)
    list(APPEND sources platform_avr.c)
    list(APPEND defines LWEXT4_TEST_RAMDISK_SIZE=0)
    set(link_flags "")
    set(runner simavr -m atmega1284 -f 16000000)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL msp430-sim)
    list(APPEND sources platform_hosted.c)
    # The simulator has 512 KiB of RAM above 64 KiB; ext3/ext4 journals
    # need at least 1024 blocks, so run the mkfs tests without them.
    list(APPEND defines LWEXT4_TEST_RAMDISK_SIZE=256ul*1024ul
                        LWEXT4_TEST_JOURNAL=0)
    set(link_flags "-msim")
    set(runner ${MSP430_PREFIX}-run)
else()
    message(FATAL_ERROR "tests/baremetal: no runner for ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_executable(baremetal_test.elf ${sources})
target_include_directories(baremetal_test.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(baremetal_test.elf PRIVATE ${defines})
target_link_libraries(baremetal_test.elf lwext4)
set_target_properties(baremetal_test.elf PROPERTIES LINK_FLAGS
    "${link_flags} -Wl,-Map=baremetal_test.map")

add_test(NAME baremetal COMMAND ${runner} $<TARGET_FILE:baremetal_test.elf>)
# simavr cannot report an exit status: the verdict is the PASS/FAIL line.
set_tests_properties(baremetal PROPERTIES
    TIMEOUT 900
    PASS_REGULAR_EXPRESSION "PASS"
    FAIL_REGULAR_EXPRESSION "FAIL")
