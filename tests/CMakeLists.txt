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
