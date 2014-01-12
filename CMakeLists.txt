project(lwext4 C)
cmake_minimum_required(VERSION 2.8)


#LIBRARY
include_directories(. lwext4)
aux_source_directory(lwext4 LWEXT4_SRC)
add_library(lwext4  ${LWEXT4_SRC})


#EXECUTABLE

if(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m3)
#Library size print
add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)

elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m4)
#Library size print
add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)

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
set_target_properties(stm324f29_demo PROPERTIES COMPILE_DEFINITIONS "STM32F429_439xx")
set_target_properties(stm324f29_demo PROPERTIES LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/demos/stm32f429_disco/stm32f429.ld")
target_link_libraries(stm324f29_demo lwext4)

add_custom_target(stm32f429_size ALL DEPENDS stm324f29_demo COMMAND ${SIZE} -B stm324f29_demo)

elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  bf518)
#Library size print
add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)

else()
#Generic example target
include_directories(blockdev/filedev)
include_directories(blockdev/io_raw)

aux_source_directory(blockdev/filedev GENERIC_SRC)
aux_source_directory(blockdev/io_raw GENERIC_SRC)
aux_source_directory(demos/generic GENERIC_SRC)

add_executable(fileimage_demo ${GENERIC_SRC})
target_link_libraries(fileimage_demo lwext4)
add_custom_target(size ALL DEPENDS lwext4 COMMAND size -B liblwext4.a)
endif()

#DISTRIBUTION
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
"/build")
include(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
