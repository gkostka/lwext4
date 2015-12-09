# Name of the target
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avrxmega7)

set(MCPU_FLAGS "-mmcu=avrxmega7")

include(${CMAKE_CURRENT_LIST_DIR}/common/avr-gcc.cmake)