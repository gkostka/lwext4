project(lwext4 C)
cmake_minimum_required(VERSION 2.8)


#LIBRARY
include_directories(. lwext4)
aux_source_directory(lwext4 LWEXT4_SRC)
add_library(lwext4  ${LWEXT4_SRC})

#Examples
if(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m3)
    #Library size print
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m4)
    #Library size print
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)
    #Discoery disco demo
    include(demos/stm32f429_disco/stm32f429_demo.cmake)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  bf518)
    #Library size print
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)
else()
    #Library size print
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND size -B liblwext4.a)
    #Generic example target
    include(demos/generic/generic.cmake)
    include(fs_test/fs_test.cmake)
endif()

#DISTRIBUTION
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "4")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
"/build" ".svn")
include(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
