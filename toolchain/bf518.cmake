# Name of the target
set(CMAKE_SYSTEM_NAME bfin-elf)
set(CMAKE_SYSTEM_PROCESSOR bf518)

set(MCPU_FLAGS "-mcpu=bf518")

include(${CMAKE_CURRENT_LIST_DIR}/common/bfin-elf.cmake)