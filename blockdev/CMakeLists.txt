#Blockdev library
aux_source_directory(filedev BLOCKDEV_SRC)
aux_source_directory(filedev_win BLOCKDEV_SRC)
add_library(blockdev  ${BLOCKDEV_SRC})

