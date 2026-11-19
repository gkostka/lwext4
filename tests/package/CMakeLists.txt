# SPDX-License-Identifier: BSD-3-Clause
#
# A separate project that uses an installed lwext4 through its CMake
# package (see install_package.sh):
#
#   cmake -S tests/package -B build -DCMAKE_PREFIX_PATH=<lwext4 prefix>
cmake_minimum_required(VERSION 3.4...3.31)
project(lwext4_consumer C)

find_package(lwext4 1.0 CONFIG REQUIRED)

add_executable(consumer consumer.c)
target_link_libraries(consumer lwext4::blockdev lwext4::lwext4)
