project(lwext4 C)
cmake_minimum_required(VERSION 2.8)


#LIBRARY
include_directories(. lwext4)
aux_source_directory(lwext4 LWEXT4_SRC)
add_library(lwext4  ${LWEXT4_SRC})


#EXECUTABLE

if(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m3)
#Library size print
add_custom_target(size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)

elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  cortex-m4)
#Library size print
add_custom_target(size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)

elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL  bf518)
#Library size print
add_custom_target(size ALL DEPENDS lwext4 COMMAND ${SIZE} -B liblwext4.a)

else()
#Generic example target
include_directories(blockdev/filedev)
aux_source_directory(blockdev/filedev FILEDEV_SRC)
aux_source_directory(demos/generic GENERIC_SRC)
add_executable(fileimage_demo ${GENERIC_SRC} ${FILEDEV_SRC})
target_link_libraries(fileimage_demo lwext4)
add_custom_target(size ALL DEPENDS lwext4 COMMAND size -B liblwext4.a)
endif()

#DISTRIBUTION
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
include(CPack)

add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
