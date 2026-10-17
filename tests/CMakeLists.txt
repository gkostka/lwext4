# Regression tests. Every tests/test_*.c becomes one executable and one CTest
# test. If a tests/test_<name>.sh exists next to it, it is run first (on the
# host) with the path of a scratch image as $1, and that image path is passed
# to the test executable as argv[1]. Setup scripts can use lwext4_mke2fs from
# common/mkimage.sh to create lwext4-compatible images.

file(GLOB LWEXT4_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)

add_library(lwext4_test_util STATIC common/test_util.c)
target_include_directories(lwext4_test_util PUBLIC common)
target_link_libraries(lwext4_test_util lwext4 blockdev)

foreach(test_src ${LWEXT4_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} lwext4_test_util lwext4 blockdev)
    add_test(NAME ${test_name}
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
                     ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.sh
                     ${CMAKE_CURRENT_BINARY_DIR}/work/${test_name}
                     ${CMAKE_CROSSCOMPILING_EMULATOR}
                     $<TARGET_FILE:${test_name}>)
endforeach()

# "cmake --install --prefix" must be honoured, so every install() rule needs
# a DESTINATION relative to CMAKE_INSTALL_PREFIX (gkostka/lwext4#48).
if(NOT CMAKE_VERSION VERSION_LESS 3.15)
    add_test(NAME install_prefix
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/install_prefix.sh
                     ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR}/work/install_prefix)
endif()

# The installed library must be usable by other projects through
# pkg-config, find_package(lwext4) and plain -I/-L flags (gkostka/lwext4#66).
# The consumers are built and run on the build machine, so not when cross
# compiling.
if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME install_package
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/install_package.sh
                     ${CMAKE_COMMAND} ${PROJECT_BINARY_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR}/work/install_package
                     ${PROJECT_SOURCE_DIR} ${CMAKE_C_COMPILER}
                     "${CMAKE_C_FLAGS}" "${CMAKE_EXE_LINKER_FLAGS}")
endif()
