include(Dependencies.cmake)
centui_sources("${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE bgfx)

include(Shaders.cmake)
centui_compile_shaders(${PROJECT_NAME})
