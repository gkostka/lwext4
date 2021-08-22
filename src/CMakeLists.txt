
option(LWEXT4_BUILD_SHARED_LIB "Build shared library" OFF)

#LIBRARY
include_directories(.)
aux_source_directory(. LWEXT4_SRC)
if(LWEXT4_BUILD_SHARED_LIB)
  add_library(lwext4 SHARED ${LWEXT4_SRC})
else()
  add_library(lwext4 STATIC ${LWEXT4_SRC})
endif()

if  (DEFINED SIZE)
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} liblwext4.a)
else()

endif()

if  (DEFINED INSTALL_LIB)
INSTALL(TARGETS lwext4 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
INSTALL(DIRECTORY ${PROJECT_BINARY_DIR}/include/. DESTINATION ${CMAKE_INSTALL_PREFIX}/include/lwext4)
endif()
