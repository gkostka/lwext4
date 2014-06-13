#Discoery disco demo
enable_language(ASM)
set (STM32F429_DEMO_ASM
    startup.S
)


include_directories(.)
include_directories(cmsis)
include_directories(stm/lcd_utils)
include_directories(stm/stm32f4_spl/inc)
include_directories(stm/stm32f429)
include_directories(stm/usb_host/Core/inc)
include_directories(stm/usb_host/Class/MSC/inc)
include_directories(stm/usb_otg/inc)
include_directories(stm/usb_user)

aux_source_directory(. STM32F429_DEMO)
aux_source_directory(cmsis STM32F429_DEMO)
aux_source_directory(stm/lcd_utils STM32F429_DEMO)
aux_source_directory(stm/stm32f4_spl/src STM32F429_DEMO)
aux_source_directory(stm/stm32f429 STM32F429_DEMO)
aux_source_directory(stm/usb_host/Core/src STM32F429_DEMO)
aux_source_directory(stm/usb_host/Class/MSC/src STM32F429_DEMO)
aux_source_directory(stm/usb_otg/src STM32F429_DEMO)
aux_source_directory(stm/usb_user STM32F429_DEMO)
add_executable(stm32f429_demo ${STM32F429_DEMO} ${STM32F429_DEMO_ASM})

add_definitions(-DSTM32F429xx)

set_target_properties(stm32f429_demo PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")
set_target_properties(stm32f429_demo PROPERTIES COMPILE_FLAGS "-Wno-format")
set_target_properties(stm32f429_demo PROPERTIES COMPILE_DEFINITIONS "STM32F429_439xx")

set_target_properties(stm32f429_demo PROPERTIES LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/demos/stm32f429_disco/stm32f429.ld")
target_link_libraries(stm32f429_demo lwext4)

add_custom_target(stm32f429_size ALL DEPENDS stm32f429_demo COMMAND ${SIZE} -B stm32f429_demo)
add_custom_target(stm32f429_bin ALL DEPENDS stm32f429_demo COMMAND ${OBJCOPY} -O binary stm32f429_demo stm32f429_demo.bin)
