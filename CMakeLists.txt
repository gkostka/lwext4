project(lwext4 C)
cmake_minimum_required(VERSION 2.8)


include_directories(lwext4)
include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)


#Library build
add_subdirectory(lwext4)
#Detect all possible warnings for lwext4 target
set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic")

#Examples
if    (CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m3)
    #cortex-m3 demos
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m4)
    #cortex-m4 demos
    add_subdirectory(demos/stm32f429_disco)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  bf518)
    #bf518 demos
    #...
else()
    #Generic example target
    add_subdirectory(blockdev)
    add_subdirectory(fs_test)
    add_subdirectory(demos/generic)
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
