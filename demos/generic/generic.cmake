include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)

aux_source_directory(blockdev/filedev GENERIC_SRC)
aux_source_directory(blockdev/filedev_win GENERIC_SRC)
aux_source_directory(demos/generic GENERIC_SRC)

add_executable(fileimage_demo ${GENERIC_SRC})
target_link_libraries(fileimage_demo lwext4)