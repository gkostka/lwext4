# SPDX-License-Identifier: BSD-3-Clause
#
# Makes the installed library usable from other build systems
# (gkostka/lwext4#66). Next to lib/liblwext4, lib/libblockdev and the
# headers in include/lwext4 this installs
#
#   lib/pkgconfig/lwext4.pc            pkg-config --cflags --libs lwext4
#   lib/cmake/lwext4/lwext4Config.cmake, lwext4ConfigVersion.cmake,
#   lwext4Targets*.cmake               find_package(lwext4 CONFIG), with the
#                                      imported targets lwext4::lwext4 and
#                                      lwext4::blockdev
#
# Both are relocatable: they locate the prefix from their own path, so
# "cmake --install --prefix" and a moved or unpacked install tree work.
#
# Only CMake 3.4 features are used (the project minimum).

include(CMakePackageConfigHelpers)

# The Makefile passes the version; plain cmake builds get the same default.
foreach(part MAJOR MINOR PATCH)
    if(NOT DEFINED VERSION_${part} OR VERSION_${part} STREQUAL "")
        if(part STREQUAL MAJOR)
            set(VERSION_${part} 1)
        else()
            set(VERSION_${part} 0)
        endif()
    endif()
endforeach()
set(LWEXT4_PACKAGE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(LWEXT4_CMAKE_DIR lib/cmake/lwext4)

install(EXPORT lwext4Targets
        NAMESPACE lwext4::
        DESTINATION ${LWEXT4_CMAKE_DIR})

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/lwext4ConfigVersion.cmake
    VERSION ${LWEXT4_PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_LIST_DIR}/lwext4Config.cmake
              ${PROJECT_BINARY_DIR}/lwext4ConfigVersion.cmake
        DESTINATION ${LWEXT4_CMAKE_DIR})

configure_file(${CMAKE_CURRENT_LIST_DIR}/lwext4.pc.in
               ${PROJECT_BINARY_DIR}/lwext4.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/lwext4.pc DESTINATION lib/pkgconfig)
