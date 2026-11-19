# SPDX-License-Identifier: BSD-3-Clause
#
# CMake package of an installed lwext4:
#
#   find_package(lwext4 1.0 CONFIG REQUIRED)
#   target_link_libraries(app lwext4::lwext4)     # the library, <ext4.h>
#   target_link_libraries(app lwext4::blockdev)   # + <blockdev/file_dev.h>
#
# lwext4::blockdev links lwext4::lwext4 itself.
include("${CMAKE_CURRENT_LIST_DIR}/lwext4Targets.cmake")
