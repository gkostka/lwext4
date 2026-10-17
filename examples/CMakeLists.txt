# SPDX-License-Identifier: BSD-3-Clause
#
# Host examples, built with the generic target (make generic); see
# README.md in this directory. ci/scripts/check-examples.sh runs them and
# checks the images they write with e2fsck and debugfs.

# The examples include the file block device as <blockdev/file_dev.h>,
# the path it has below an installed include/lwext4. Mirror that here.
configure_file(${PROJECT_SOURCE_DIR}/blockdev/linux/file_dev.h
               ${CMAKE_CURRENT_BINARY_DIR}/include/blockdev/file_dev.h
               COPYONLY)

add_executable(lwext4-example-basic basic/main.c)
target_include_directories(lwext4-example-basic PRIVATE
                           ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(lwext4-example-basic blockdev lwext4)

add_executable(lwext4-example-blockdev-template
               blockdev-template/main.c
               blockdev-template/my_blockdev.c
               blockdev-template/ram_storage.c)
target_link_libraries(lwext4-example-blockdev-template lwext4)
