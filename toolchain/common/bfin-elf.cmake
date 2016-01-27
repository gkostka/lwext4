# Toolchain settings
set(CMAKE_C_COMPILER    bfin-elf-gcc)
set(CMAKE_CXX_COMPILER  bfin-elf-g++)
set(AS                  bfin-elf--gcc)
set(AR                  bfin-elf-ar)
set(OBJCOPY             bfin-elf-objcopy)
set(OBJDUMP             bfin-elf-objdump)
set(SIZE                bfin-elf-size)

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