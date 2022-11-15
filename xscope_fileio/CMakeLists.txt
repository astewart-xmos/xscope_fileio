
set( LIB_NAME xscope_fileio )

add_library( ${LIB_NAME} STATIC )

target_sources( ${LIB_NAME} 
    PUBLIC
      src/xscope_io_device.c 
)

target_include_directories( ${LIB_NAME}
    PUBLIC
      api
      api/..
)

target_compile_options( ${LIB_NAME} 
    PRIVATE
      -g -Os 
      -fxscope
)

install(
  FILES "__init__.py"
  DESTINATION ${WORKSPACE_PATH}/xscope_fileio
)