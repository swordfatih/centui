include(Dependencies.cmake)
centui_module_library(centui_sfml centui::sfml "${PROJECT_SOURCE_DIR}/src/backends/sfml/backend.cppm")
target_link_libraries(centui_sfml PUBLIC centui_core SFML::Graphics)
