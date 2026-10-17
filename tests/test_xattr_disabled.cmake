# SPDX-License-Identifier: BSD-3-Clause
# README.md: "To use library as a BSD3, GPLv2 licensed source files must be
# removed first". Build the library that way, without ext4_extent.c and
# ext4_xattr.c and with the features they implement disabled, and link the
# test against it instead of the full library.
file(GLOB lwext4_no_gpl_src ${PROJECT_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM lwext4_no_gpl_src
     ${PROJECT_SOURCE_DIR}/src/ext4_extent.c
     ${PROJECT_SOURCE_DIR}/src/ext4_xattr.c)
add_library(lwext4_no_gpl STATIC ${lwext4_no_gpl_src})
target_include_directories(lwext4_no_gpl PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(lwext4_no_gpl PUBLIC
                           CONFIG_EXTENTS_ENABLE=0 CONFIG_XATTR_ENABLE=0)

target_sources(${test_name} PRIVATE common/test_util.c)
target_include_directories(${test_name} PRIVATE common)
set_property(TARGET ${test_name} PROPERTY LINK_LIBRARIES lwext4_no_gpl blockdev)
