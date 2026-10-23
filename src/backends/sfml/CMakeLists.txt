add_library(centui_sfml STATIC)
add_library(centui::sfml ALIAS centui_sfml)
centui_sources(centui_sfml "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/centui/backends/sfml.hpp")
target_link_libraries(centui_sfml PUBLIC centui::centui centui::sfml_platform SFML::Graphics)
