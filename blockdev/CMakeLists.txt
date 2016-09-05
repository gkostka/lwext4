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

