if(NOT CENTUI_BUILD_TESTS)
    return()
endif()

function(centui_integration name platform backend platform_target backend_target)
    if(NOT TARGET ${platform_target} OR NOT TARGET ${backend_target})
        return()
    endif()

    set(target centui_integration_${name})

    add_executable(${target} main.cpp)
    target_compile_definitions(${target} PRIVATE
        CENTUI_PLATFORM_HEADER="centui/platforms/${platform}.hpp"
        CENTUI_BACKEND_HEADER="centui/backends/${backend}.hpp"
        CENTUI_PLATFORM_TYPE=centui::${platform}::platform
        CENTUI_BACKEND_TYPE=centui::${backend}::backend
    )
    target_link_libraries(${target} PRIVATE
        ${platform_target}
        ${backend_target}
    )
    centui_font_asset(${target})

    add_test(NAME integration_${name} COMMAND ${target})
    set_tests_properties(integration_${name} PROPERTIES TIMEOUT 30 LABELS integration)
endfunction()

centui_integration(sdl sdl sdl centui::platform::sdl centui::backend::sdl)
centui_integration(sfml sfml sfml centui::platform::sfml centui::backend::sfml)
centui_integration(sdl_bgfx sdl bgfx centui::platform::sdl centui::backend::bgfx)
centui_integration(glfw_bgfx glfw bgfx centui::platform::glfw centui::backend::bgfx)
