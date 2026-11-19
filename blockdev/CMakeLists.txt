#Blockdev library

if    (WIN32)
  aux_source_directory(linux BLOCKDEV_SRC)
  aux_source_directory(windows BLOCKDEV_SRC)
elseif    (BLOCKDEV_TYPE STREQUAL  linux)
  aux_source_directory(linux BLOCKDEV_SRC)
else()
endif()

aux_source_directory(. BLOCKDEV_SRC)
add_library(blockdev  ${BLOCKDEV_SRC})


if(DEFINED INSTALL_LIB)
  # The file block devices (file_dev.h, file_windows.h on Windows) and the
  # blockdev.h skeleton, for programs built against the installed library:
  #   #include <blockdev/file_dev.h>
  set(BLOCKDEV_HEADERS blockdev.h)
  if(WIN32 OR BLOCKDEV_TYPE STREQUAL linux)
    list(APPEND BLOCKDEV_HEADERS linux/file_dev.h)
  endif()
  if(WIN32)
    list(APPEND BLOCKDEV_HEADERS windows/file_windows.h)
  endif()
  install(FILES ${BLOCKDEV_HEADERS} DESTINATION include/lwext4/blockdev)
  # Its headers include the lwext4 ones
  target_link_libraries(blockdev PUBLIC lwext4)
  install(TARGETS blockdev EXPORT lwext4Targets
          ARCHIVE DESTINATION lib
          LIBRARY DESTINATION lib
          RUNTIME DESTINATION bin)
endif()
