# Toolchain settings
# The old mspgcc port is called msp430-gcc, TI's current GCC msp430-elf-gcc.
if(NOT MSP430_PREFIX)
    find_program(MSP430_LEGACY_GCC msp430-gcc)
    if(MSP430_LEGACY_GCC)
        set(MSP430_PREFIX msp430)
    else()
        set(MSP430_PREFIX msp430-elf)
    endif()
endif()
set(CMAKE_C_COMPILER    ${MSP430_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER  ${MSP430_PREFIX}-g++)
set(AS                  ${MSP430_PREFIX}-as)
set(AR                  ${MSP430_PREFIX}-ar)
set(OBJCOPY             ${MSP430_PREFIX}-objcopy)
set(OBJDUMP             ${MSP430_PREFIX}-objdump)
set(SIZE                ${MSP430_PREFIX}-size)

# The -mmcu link step pulls in the device linker script, which clashes when
# CMake's compiler check links a test program; a static library is enough.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# TI's device support files (devices.csv, headers, linker scripts).
if(DEFINED ENV{MSP430_SUPPORT})
    set(MCPU_FLAGS "${MCPU_FLAGS} -I$ENV{MSP430_SUPPORT} -L$ENV{MSP430_SUPPORT}")
endif()

set(CMAKE_C_FLAGS   "${MCPU_FLAGS} -Wall -std=gnu99 -fdata-sections -ffunction-sections" CACHE INTERNAL "c compiler flags")
set(CMAKE_CXX_FLAGS "${MCPU_FLAGS} -Wall -fdata-sections -ffunction-sections" CACHE INTERNAL "cxx compiler flags")
set(CMAKE_ASM_FLAGS "${MCPU_FLAGS} -x assembler-with-cpp" CACHE INTERNAL "asm compiler flags")
set(CMAKE_EXE_LINKER_FLAGS "${MCPU_FLAGS} -nostartfiles -Wl,--gc-sections" CACHE INTERNAL "exe link flags")


SET(CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb3" CACHE INTERNAL "c debug compiler flags")
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb3" CACHE INTERNAL "cxx debug compiler flags")
SET(CMAKE_ASM_FLAGS_DEBUG "-g -ggdb3" CACHE INTERNAL "asm debug compiler flags")

SET(CMAKE_C_FLAGS_RELEASE "-Os -g -ggdb3" CACHE INTERNAL "c release compiler flags")
SET(CMAKE_CXX_FLAGS_RELEASE "-Os -g -ggdb3" CACHE INTERNAL "cxx release compiler flags")
SET(CMAKE_ASM_FLAGS_RELEASE "" CACHE INTERNAL "asm release compiler flags")
