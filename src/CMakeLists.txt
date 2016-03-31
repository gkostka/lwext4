
#LIBRARY
include_directories(.)
aux_source_directory(. LWEXT4_SRC)
add_library(lwext4 STATIC ${LWEXT4_SRC})

if  (DEFINED SIZE)
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} liblwext4.a)
else()

endif()

if  (DEFINED INSTALL_LIB)
INSTALL(TARGETS lwext4 DESTINATION /usr/local/lib)
INSTALL(DIRECTORY ${PROJECT_BINARY_DIR}/include/. DESTINATION /usr/local/include/lwext4)
endif()