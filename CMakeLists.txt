project(lwext4 C)
cmake_minimum_required(VERSION 3.4)


include_directories(include)
include_directories(${PROJECT_BINARY_DIR}/include)
include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)

set(BLOCKDEV_TYPE  none)

add_definitions(-DCONFIG_USE_DEFAULT_CONFIG=0)
add_definitions(-DVERSION="${VERSION}")

#Examples
if    (CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m0)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m3)
    add_definitions(-DCONFIG_UNALIGNED_ACCESS=1)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  arm-sim)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m4)
    add_definitions(-DCONFIG_UNALIGNED_ACCESS=1)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  bf518)
    #...
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  avrxmega7)
    add_definitions(-DCONFIG_HAVE_OWN_ERRNO=1)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  msp430g2210)
    add_definitions(-DCONFIG_DEBUG_PRINTF=0)
    add_definitions(-DCONFIG_DEBUG_ASSERT=0)
    #...
elseif(LIB_ONLY)
    add_definitions(-DCONFIG_DEBUG_PRINTF=0)
    add_definitions(-DCONFIG_DEBUG_ASSERT=0)
    add_definitions(-DCONFIG_HAVE_OWN_OFLAGS=1)
    add_definitions(-DCONFIG_HAVE_OWN_ERRNO=0)
    add_definitions(-DCONFIG_BLOCK_DEV_CACHE_SIZE=16)
else()
    #Generic example target
    if (WIN32)
      set(BLOCKDEV_TYPE  windows)
    else()
      set(BLOCKDEV_TYPE  linux)
    endif()
    set (INSTALL_LIB 1)
    add_definitions(-DCONFIG_HAVE_OWN_OFLAGS=0)
    add_definitions(-DCONFIG_HAVE_OWN_ERRNO=0)
    add_definitions(-DCONFIG_HAVE_OWN_ASSERT=0)
    add_definitions(-DCONFIG_BLOCK_DEV_CACHE_SIZE=16)
    add_subdirectory(fs_test)
endif()

macro(output_configure)
    get_property(
        definitions
        DIRECTORY
        PROPERTY COMPILE_DEFINITIONS
    )
    file(WRITE
         ${PROJECT_BINARY_DIR}/include/generated/ext4_config.h
         "")
    foreach(item ${definitions})
        string(REGEX MATCH "^CONFIG_" match_res ${item})
        if(match_res)
            string(REGEX REPLACE "=(.+)$" "" replace_res ${item})
            string(CONFIGURE
                   "#define ${replace_res} ${CMAKE_MATCH_1}"
                   output_str)
            file(APPEND
                 ${PROJECT_BINARY_DIR}/include/generated/ext4_config.h
                 "${output_str}\n")
        endif()
    endforeach()
endmacro()
output_configure()

add_subdirectory(blockdev)

#Library build
add_subdirectory(src)
#Detect all possible warnings for lwext4 target
if (NOT CMAKE_COMPILER_IS_GNUCC)
    set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "")
else()
    set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic")
endif()

#DISTRIBUTION
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
"/build" ".git")
include(CPack)


add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
