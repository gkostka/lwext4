# Name of the target
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR msp430g2210)

set(MCPU_FLAGS "-mmcu=msp430g2210")

include(${CMAKE_CURRENT_LIST_DIR}/common/msp430-gcc.cmake)