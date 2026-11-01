if(NOT CENTUI_BUILD_TESTS)
    return()
endif()

function(centui_integration name platform backend platform_target backend_target)
    if(NOT TARGET ${platform_target} OR NOT TARGET ${backend_target})
        return()
    endif()

    set(target centui_integration_${name})

    add_executable(${target} platform_renderer.test.cpp)
    target_compile_definitions(${target} PRIVATE
        CENTUI_PLATFORM_HEADER="centui/platforms/${platform}.hpp"
        CENTUI_BACKEND_HEADER="centui/backends/${backend}.hpp"
        CENTUI_PLATFORM_TYPE=centui::${platform}::platform
        CENTUI_BACKEND_TYPE=centui::${backend}::backend
    )
    target_link_libraries(${target} PRIVATE
        ${platform_target}
        ${backend_target}
        Catch2::Catch2WithMain
    )
    centui_font_asset(${target})

    add_test(NAME integration_${name} COMMAND ${target})
    set_tests_properties(integration_${name} PROPERTIES TIMEOUT 30 LABELS integration)
endfunction()

centui_integration(sdl sdl sdl centui::platform::sdl centui::backend::sdl)
centui_integration(sfml sfml sfml centui::platform::sfml centui::backend::sfml)
centui_integration(sdl_bgfx sdl bgfx centui::platform::sdl centui::backend::bgfx)
centui_integration(glfw_bgfx glfw bgfx centui::platform::glfw centui::backend::bgfx)

if(NOT EMSCRIPTEN)
    foreach(adapter sdl glfw)
        if(TARGET centui::platform::${adapter})
            add_executable(centui_${adapter}_events_tests ${adapter}_events.test.cpp)
            target_link_libraries(centui_${adapter}_events_tests PRIVATE centui::platform::${adapter} Catch2::Catch2WithMain)
            add_test(NAME ${adapter}_events COMMAND centui_${adapter}_events_tests)
            set_tests_properties(${adapter}_events PROPERTIES TIMEOUT 30)
        endif()
    endforeach()
    if(TARGET centui_sdl_events_tests)
        set_tests_properties(sdl_events PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy")
    endif()
    if(TARGET centui::backend::bgfx)
        add_executable(centui_bgfx_draw_tests bgfx_draw.test.cpp)
        target_link_libraries(centui_bgfx_draw_tests PRIVATE centui::backend::bgfx Catch2::Catch2WithMain)
        add_test(NAME bgfx_draw COMMAND centui_bgfx_draw_tests)
        set_tests_properties(bgfx_draw PROPERTIES TIMEOUT 30)
    endif()
endif()
