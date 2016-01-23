
#LIBRARY
include_directories(.)
aux_source_directory(. LWEXT4_SRC)
add_library(lwext4  ${LWEXT4_SRC})
if  (DEFINED SIZE)
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} liblwext4.a)
else()

endif()
