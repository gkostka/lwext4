# SPDX-License-Identifier: BSD-3-Clause
# The setup script configures lwext4 again as a shared library with this
# build's toolchain (whose SIZE setting adds the lib_size target) and
# builds it. Hand it the toolchain and the cross compilation settings.
set(cache ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.cache.cmake)
file(WRITE ${cache} "")
if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(toolchain ${CMAKE_TOOLCHAIN_FILE} ABSOLUTE
                           BASE_DIR ${CMAKE_BINARY_DIR})
    if(NOT EXISTS ${toolchain})
        get_filename_component(toolchain ${CMAKE_TOOLCHAIN_FILE} ABSOLUTE
                               BASE_DIR ${PROJECT_SOURCE_DIR})
    endif()
    file(APPEND ${cache}
         "set(CMAKE_TOOLCHAIN_FILE [[${toolchain}]] CACHE FILEPATH \"\")\n")
else()
    file(APPEND ${cache}
         "set(CMAKE_C_COMPILER [[${CMAKE_C_COMPILER}]] CACHE FILEPATH \"\")\n")
endif()
if(NOT DEFINED SIZE)
    set(SIZE size)
endif()
file(APPEND ${cache} "set(SIZE [[${SIZE}]] CACHE STRING \"\")\n")
get_cmake_property(cache_vars CACHE_VARIABLES)
foreach(var ${cache_vars})
    if(var MATCHES "^CROSS_")
        file(APPEND ${cache} "set(${var} [[${${var}}]] CACHE STRING \"\")\n")
    endif()
endforeach()
set_tests_properties(${test_name} PROPERTIES ENVIRONMENT
    "LWEXT4_SOURCE_DIR=${PROJECT_SOURCE_DIR};LWEXT4_INITIAL_CACHE=${cache};LWEXT4_GENERATOR=${CMAKE_GENERATOR}")
