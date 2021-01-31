set (_projname "ColorizedBodyViewer-SFML")

set (${_projname}_SOURCES
  ${SAMPLE_COMMON_DIR}/LitDepthVisualizer.hpp
  main.cpp
  ${SAMPLE_COMMON_DIR}/key_handler.h
  )

find_package(SFML COMPONENTS graphics window system)

add_executable(${_projname} ${${_projname}_SOURCES})

set_target_properties(${_projname} PROPERTIES FOLDER "${SAMPLE_DIR_FOLDER}sfml")

include_directories(${SFML_INCLUDE_DIR})

target_link_libraries(${_projname} ${ASTRA_LIBRARIES} ${SFML_LIBRARIES})

set_debug_working_dir(${_projname} "$(TargetDir)")

add_astra_sdk_dependency(${_projname})

install_sample(${_projname})
