project(lwext4 C)
cmake_minimum_required(VERSION 2.8)


include_directories(lwext4)
include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)

set(BLOCKDEV_TYPE  none)

#Examples
if    (CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m0)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m3)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  arm-sim)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m4)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  bf518)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  avrxmega7)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  msp430g2210)
    add_definitions(-DCONFIG_DEBUG_PRINTF=0)
    add_definitions(-DCONFIG_DEBUG_ASSERT=0)
    #...
else()
    #Generic example target
    set(BLOCKDEV_TYPE  linux)
    add_definitions(-DCONFIG_HAVE_OWN_OFLAGS=0)
    add_definitions(-DCONFIG_HAVE_OWN_ERRNO=0)
    add_definitions(-DCONFIG_HAVE_OWN_ASSERT=0)
    add_definitions(-DCONFIG_BLOCK_DEV_CACHE_SIZE=16)
    add_subdirectory(fs_test)
endif()

add_subdirectory(blockdev)

#Library build
add_subdirectory(lwext4)
#Detect all possible warnings for lwext4 target
set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic")


#DISTRIBUTION
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "8")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
"/build" ".git")
include(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
