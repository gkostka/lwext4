# Name of the target
SET(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

# Toolchain settings
set(CMAKE_C_COMPILER    arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER  arm-none-eabi-g++)
set(AS                  arm-none-eabi-as)
set(AR                  arm-none-eabi-ar)
set(OBJCOPY             arm-none-eabi-objcopy)
set(OBJDUMP             arm-none-eabi-objdump)
set(SIZE                arm-none-eabi-size)

set(CMAKE_C_FLAGS   "-mthumb -mcpu=cortex-m3 -Os -fno-builtin -Wall -std=gnu99 -fdata-sections -ffunction-sections" CACHE INTERNAL "c compiler flags")
set(CMAKE_CXX_FLAGS "-mthumb -mcpu=cortex-m3 -Os -fno-builtin -Wall -fdata-sections -ffunction-sections" CACHE INTERNAL "cxx compiler flags")
set(CMAKE_ASM_FLAGS "-mthumb -mcpu=cortex-m3" CACHE INTERNAL "asm compiler flags")
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -Wl,--gc-sections -mthumb -mcpu=cortex-m3" CACHE INTERNAL "exe link flags")

