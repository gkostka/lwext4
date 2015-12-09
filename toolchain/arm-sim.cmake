# Name of the target
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm-sim)

set(MCPU_FLAGS "")
set(VFP_FLAGS "")
set(LD_FLAGS "--specs=rdimon.specs -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group")

include(${CMAKE_CURRENT_LIST_DIR}/common/arm-none-eabi.cmake)
