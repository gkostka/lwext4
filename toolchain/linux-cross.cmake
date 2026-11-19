# SPDX-License-Identifier: BSD-3-Clause
# Cross compile the generic (Linux file blockdev) target for another Linux
# architecture, e.g. to run the regression tests under qemu-user.
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/linux-cross.cmake \
#         -DCROSS_TRIPLE=s390x-linux-gnu \
#         -DCROSS_EMULATOR=qemu-s390x ..
#
# CROSS_TRIPLE selects <triple>-gcc. CROSS_EMULATOR (optional) becomes
# CMAKE_CROSSCOMPILING_EMULATOR, with -L pointing at the cross sysroot
# (CROSS_SYSROOT, default /usr/<triple>) so qemu-user finds the dynamic
# loader and libc. CROSS_CFLAGS (optional) is appended to the C flags, e.g.
# -DCROSS_CFLAGS=-DCONFIG_BIG_ENDIAN=1 to force the byte order, and
# CROSS_LDFLAGS (optional) to the executable link flags, e.g. -static. All
# of them can also be given as environment variables.

if(NOT CROSS_TRIPLE)
    set(CROSS_TRIPLE "$ENV{CROSS_TRIPLE}")
endif()
if(NOT CROSS_TRIPLE)
    message(FATAL_ERROR "linux-cross.cmake: set CROSS_TRIPLE, e.g. -DCROSS_TRIPLE=s390x-linux-gnu")
endif()
if(NOT CROSS_EMULATOR)
    set(CROSS_EMULATOR "$ENV{CROSS_EMULATOR}")
endif()
if(NOT CROSS_CFLAGS)
    set(CROSS_CFLAGS "$ENV{CROSS_CFLAGS}")
endif()
if(NOT CROSS_LDFLAGS)
    set(CROSS_LDFLAGS "$ENV{CROSS_LDFLAGS}")
endif()
if(NOT CROSS_SYSROOT)
    set(CROSS_SYSROOT "/usr/${CROSS_TRIPLE}")
endif()
# try_compile() projects re-read this file without the -D cache entries.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CROSS_TRIPLE CROSS_EMULATOR CROSS_SYSROOT CROSS_CFLAGS CROSS_LDFLAGS)

set(CMAKE_SYSTEM_NAME Linux)
# Must not match any of the embedded CMAKE_SYSTEM_PROCESSOR names in the top
# level CMakeLists.txt, so the generic target (and the tests) is configured.
string(REGEX REPLACE "-.*$" "" CMAKE_SYSTEM_PROCESSOR "${CROSS_TRIPLE}")

set(CMAKE_C_COMPILER    ${CROSS_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER  ${CROSS_TRIPLE}-g++)
set(AS                  ${CROSS_TRIPLE}-as)
set(AR                  ${CROSS_TRIPLE}-ar)
set(OBJCOPY             ${CROSS_TRIPLE}-objcopy)
set(OBJDUMP             ${CROSS_TRIPLE}-objdump)
set(SIZE                ${CROSS_TRIPLE}-size)

set(CMAKE_FIND_ROOT_PATH ${CROSS_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

if(CROSS_EMULATOR)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${CROSS_EMULATOR} -L ${CROSS_SYSROOT})
endif()

set(CMAKE_C_FLAGS   "-std=gnu99 -fdata-sections -ffunction-sections ${CROSS_CFLAGS}" CACHE INTERNAL "c compiler flags")
set(CMAKE_CXX_FLAGS "-fdata-sections -ffunction-sections" CACHE INTERNAL "cxx compiler flags")
set(CMAKE_ASM_FLAGS "" CACHE INTERNAL "asm compiler flags")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections ${CROSS_LDFLAGS}" CACHE INTERNAL "exe link flags")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb3" CACHE INTERNAL "c debug compiler flags")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb3" CACHE INTERNAL "cxx debug compiler flags")
set(CMAKE_ASM_FLAGS_DEBUG "-g -ggdb3" CACHE INTERNAL "asm debug compiler flags")

set(CMAKE_C_FLAGS_RELEASE "-O2 -g -ggdb3" CACHE INTERNAL "c release compiler flags")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g -ggdb3" CACHE INTERNAL "cxx release compiler flags")
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE INTERNAL "asm release compiler flags")
