function(centui_render_test target source)
    # Static window libraries can export colliding native protocol symbols.
    # Each executable exercises one supported pairing independently.
    centui_test(${target} "${source}" PROPERTIES TIMEOUT 30 LABELS integration)
    target_link_libraries(${target} PRIVATE ${ARGN})
    target_compile_definitions(${target} PRIVATE
        CENTUI_FONT_PATH="${PROJECT_SOURCE_DIR}/tests/assets/fonts/Inter-Regular.ttf"
    )
endfunction()

if(TARGET centui::backend::sdl)
    centui_render_test(centui_sdl_renderer_tests sdl_sdl.test.cpp centui::backend::sdl)
endif()
if(TARGET centui::backend::sfml)
    centui_render_test(centui_sfml_renderer_tests sfml_sfml.test.cpp centui::backend::sfml)
endif()
if(TARGET centui::platform::sdl AND TARGET centui::backend::bgfx)
    centui_render_test(centui_sdl_bgfx_tests sdl_bgfx.test.cpp centui::platform::sdl centui::backend::bgfx)
endif()
if(TARGET centui::platform::glfw AND TARGET centui::backend::bgfx)
    centui_render_test(centui_glfw_bgfx_tests glfw_bgfx.test.cpp centui::platform::glfw centui::backend::bgfx)
endif()

if(TARGET centui::platform::sdl)
    centui_test(centui_sdl_events_tests sdl_events.test.cpp
        PROPERTIES TIMEOUT 30 LABELS integration ENVIRONMENT "SDL_VIDEO_DRIVER=dummy")
    target_link_libraries(centui_sdl_events_tests PRIVATE centui::platform::sdl)
endif()
if(TARGET centui::platform::glfw)
    centui_test(centui_glfw_events_tests glfw_events.test.cpp PROPERTIES TIMEOUT 30 LABELS integration)
    target_link_libraries(centui_glfw_events_tests PRIVATE centui::platform::glfw)
endif()
if(TARGET centui::backend::bgfx)
    centui_test(centui_bgfx_draw_tests bgfx_draw.test.cpp PROPERTIES TIMEOUT 30 LABELS integration)
    target_link_libraries(centui_bgfx_draw_tests PRIVATE centui::backend::bgfx)
endif()
