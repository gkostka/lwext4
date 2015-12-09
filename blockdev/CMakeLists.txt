#Blockdev library

if    (BLOCKDEV_TYPE STREQUAL  linux)
aux_source_directory(linux BLOCKDEV_SRC)
elseif    (BLOCKDEV_TYPE STREQUAL  windows)
aux_source_directory(windows BLOCKDEV_SRC)
else()
endif()

aux_source_directory(. BLOCKDEV_SRC)
add_library(blockdev  ${BLOCKDEV_SRC})

