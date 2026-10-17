# Regression tests. Every tests/test_*.c becomes one executable and one CTest
# test. If a tests/test_<name>.sh exists next to it, it is run first (on the
# host) with the path of a scratch image as $1, and that image path is passed
# to the test executable as argv[1]. Setup scripts can use lwext4_mke2fs from
# common/mkimage.sh to create lwext4-compatible images. A tests/test_<name>.cmake
# next to it is included after the test is defined (with test_name set), for
# tests that need the library built or run differently.

file(GLOB LWEXT4_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)

add_library(lwext4_test_util STATIC common/test_util.c)
target_include_directories(lwext4_test_util PUBLIC common)
target_link_libraries(lwext4_test_util lwext4 blockdev)

# Helper program of the README acceptance tests (not a CTest test).
add_subdirectory(acceptance)

# Tests that use POSIX APIs or run host tools (e2fsck) through system()
# cannot run as Windows programs. On Windows they are registered as
# DISABLED, so CTest lists them as "Not Run (Disabled)" instead of hiding
# them; every other platform runs them.
set(LWEXT4_POSIX_ONLY_REGEX
    "#include <(unistd|fcntl|sys/[a-z]+)\\.h>|[^a-z_]system\\(|[^a-z_]alarm\\(")

foreach(test_src ${LWEXT4_TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    if(WIN32)
        file(READ ${test_src} test_text)
        if(test_text MATCHES "${LWEXT4_POSIX_ONLY_REGEX}")
            message(STATUS "${test_name}: needs a POSIX host, disabled on Windows")
            add_test(NAME ${test_name} COMMAND ${CMAKE_COMMAND} -E false)
            set_tests_properties(${test_name} PROPERTIES DISABLED TRUE)
            continue()
        endif()
    endif()
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} lwext4_test_util lwext4 blockdev)
    add_test(NAME ${test_name}
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
                     ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.sh
                     ${CMAKE_CURRENT_BINARY_DIR}/work/${test_name}
                     ${CMAKE_CROSSCOMPILING_EMULATOR}
                     $<TARGET_FILE:${test_name}>)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cmake)
        include(${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cmake)
    endif()
endforeach()
