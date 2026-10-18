include(Dependencies.cmake)
centui_module_library(centui_app centui::app
    "${PROJECT_SOURCE_DIR}/src/app/window.cppm"
    "${PROJECT_SOURCE_DIR}/src/app/app.cppm")
target_link_libraries(centui_app PUBLIC centui_bgfx PRIVATE SDL3::SDL3 bgfx)
