# Name of the target
SET(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR bf518)

# Toolchain settings
set(CMAKE_C_COMPILER 	bfin-elf-gcc)
set(CMAKE_CXX_COMPILER	bfin-elf-g++)
set(AS                  bfin-elf--gcc)
set(AR			        bfin-elf-ar)
set(OBJCOPY 		    bfin-elf-objcopy)
set(OBJDUMP 		    bfin-elf-objdump)
set(SIZE                bfin-elf-size)

set(CMAKE_C_FLAGS   "-mcpu=bf518 -Wall -std=gnu99 -fdata-sections -ffunction-sections" CACHE INTERNAL "c compiler flags")
set(CMAKE_CXX_FLAGS "-mcpu=bf518 -fno-builtin -Wall -fdata-sections -ffunction-sections" CACHE INTERNAL "cxx compiler flags")
set(CMAKE_ASM_FLAGS "-mcpu=bf518 -x assembler-with-cpp" CACHE INTERNAL "asm compiler flags")
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -Wl,--gc-sections -mcpu=bf592" CACHE INTERNAL "exe link flags")
					
							
		
		
		