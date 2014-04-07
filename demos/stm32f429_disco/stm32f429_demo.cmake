#Discoery disco demo
enable_language(ASM)
set (STM32F429_DEMO_ASM
    demos/stm32f429_disco/startup.S
)


include_directories(demos/stm32f429_disco)
include_directories(demos/stm32f429_disco/cmsis)
include_directories(demos/stm32f429_disco/stm/lcd_utils)
include_directories(demos/stm32f429_disco/stm/stm32f4_spl/inc)
include_directories(demos/stm32f429_disco/stm/stm32f429)
include_directories(demos/stm32f429_disco/stm/usb_dev/Core/inc)
include_directories(demos/stm32f429_disco/stm/usb_host/Core/inc)
include_directories(demos/stm32f429_disco/stm/usb_host/Class/MSC/inc)
include_directories(demos/stm32f429_disco/stm/usb_otg/inc)
include_directories(demos/stm32f429_disco/stm/usb_user)

aux_source_directory(demos/stm32f429_disco STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/cmsis STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/lcd_utils STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/stm32f4_spl/src STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/stm32f429 STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/usb_host/Core/src STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/usb_host/Class/MSC/src STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/usb_otg/src STM32F429_DEMO)
aux_source_directory(demos/stm32f429_disco/stm/usb_user STM32F429_DEMO)

add_executable(stm324f29_demo ${STM32F429_DEMO} ${STM32F429_DEMO_ASM})

set_target_properties(stm324f29_demo PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")
set_target_properties(stm324f29_demo PROPERTIES COMPILE_FLAGS "-Wno-format")
set_target_properties(stm324f29_demo PROPERTIES COMPILE_DEFINITIONS "STM32F429_439xx")

set_target_properties(stm324f29_demo PROPERTIES LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/demos/stm32f429_disco/stm32f429.ld")
target_link_libraries(stm324f29_demo lwext4)

add_custom_target(stm32f429_size ALL DEPENDS stm324f29_demo COMMAND ${SIZE} -B stm324f29_demo)