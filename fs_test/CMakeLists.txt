#fs_test executables
add_executable(lwext4-server lwext4_server.c)
target_link_libraries(lwext4-server lwext4)
target_link_libraries(lwext4-server blockdev)
if(WIN32)
target_link_libraries(lwext4-server ws2_32)
endif(WIN32)
add_executable(lwext4-client lwext4_client.c)
target_link_libraries(lwext4-client lwext4)
if(WIN32)
target_link_libraries(lwext4-client ws2_32)
endif(WIN32)

aux_source_directory(common COMMON_SRC)
add_executable(lwext4-generic lwext4_generic.c ${COMMON_SRC})
target_link_libraries(lwext4-generic blockdev)
target_link_libraries(lwext4-generic lwext4)

add_executable(lwext4-mkfs lwext4_mkfs.c)
target_link_libraries(lwext4-mkfs blockdev)
target_link_libraries(lwext4-mkfs lwext4)

add_executable(lwext4-mbr lwext4_mbr.c)
target_link_libraries(lwext4-mbr blockdev)
target_link_libraries(lwext4-mbr lwext4)

install (TARGETS lwext4-server DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install (TARGETS lwext4-client DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install (TARGETS lwext4-generic DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install (TARGETS lwext4-mkfs DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install (TARGETS lwext4-mbr DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

