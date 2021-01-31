find_program(DOXYGEN_EXECUTABLE
  doxygen
  PATHS /usr/local/bin)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(doxygen DEFAULT_MSG DOXYGEN_EXECUTABLE)