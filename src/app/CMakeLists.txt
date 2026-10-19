include(Dependencies.cmake)
centui_sources("${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 bgfx)
