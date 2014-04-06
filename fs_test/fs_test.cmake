include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)

aux_source_directory(blockdev/filedev BLOCKDEV_SRC)
aux_source_directory(blockdev/filedev_win BLOCKDEV_SRC)


add_executable(lwext4_server fs_test/lwext4_server.c ${BLOCKDEV_SRC})
target_link_libraries(lwext4_server lwext4)
target_link_libraries(lwext4_server ws2_32)

add_executable(lwext4_client fs_test/lwext4_client.c ${BLOCKDEV_SRC})
target_link_libraries(lwext4_client lwext4)
target_link_libraries(lwext4_client ws2_32)