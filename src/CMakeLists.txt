
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
    add_custom_target(lib_size ALL DEPENDS lwext4 COMMAND ${SIZE} $<TARGET_FILE:lwext4>)
else()

endif()

if  (DEFINED INSTALL_LIB)
# Consumers of the installed library (cmake/package.cmake) include
# <ext4.h> from include/lwext4.
target_include_directories(lwext4 INTERFACE
                           $<INSTALL_INTERFACE:include/lwext4>)
INSTALL(TARGETS lwext4 EXPORT lwext4Targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/include/. ${PROJECT_BINARY_DIR}/include/.
        DESTINATION include/lwext4)
endif()
