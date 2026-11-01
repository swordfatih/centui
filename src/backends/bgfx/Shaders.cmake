
include_guard(GLOBAL)

function(centui_compile_shaders target)
    set(shader_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders")
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
    set(include_dir "${bgfx_cmake_SOURCE_DIR}/bgfx/src")

    file(GLOB_RECURSE vertex_shaders CONFIGURE_DEPENDS "${shader_dir}/vs_*.sc")
    file(GLOB_RECURSE fragment_shaders CONFIGURE_DEPENDS "${shader_dir}/fs_*.sc")

    if(vertex_shaders)
        bgfx_compile_shaders(
            TYPE VERTEX
            SHADERS ${vertex_shaders}
            VARYING_DEF "${shader_dir}/varying.def.sc"
            OUTPUT_DIR "${output_dir}"
            INCLUDE_DIRS "${include_dir}"
            AS_HEADERS
            OUT_FILES_VAR vertex_outputs
        )
    endif()

    if(fragment_shaders)
        bgfx_compile_shaders(
            TYPE FRAGMENT
            SHADERS ${fragment_shaders}
            VARYING_DEF "${shader_dir}/varying.def.sc"
            OUTPUT_DIR "${output_dir}"
            INCLUDE_DIRS "${include_dir}"
            AS_HEADERS
            OUT_FILES_VAR fragment_outputs
        )
    endif()

    set(shader_outputs ${vertex_outputs} ${fragment_outputs})
    set(shader_header "#pragma once\n\n")
    foreach(format DXBC DXIL ESSL GLSL METAL NVN PSSL SPIRV WGSL)
        string(TOLOWER "${format}" directory)
        set(available 0)
        foreach(output IN LISTS shader_outputs)
            file(RELATIVE_PATH relative "${output_dir}" "${output}")
            if(relative MATCHES "^${directory}/")
                set(available 1)
                break()
            endif()
        endforeach()
        string(APPEND shader_header "#define BGFX_PLATFORM_SUPPORTS_${format} ${available}\n")
    endforeach()
    foreach(output IN LISTS shader_outputs)
        file(RELATIVE_PATH relative "${output_dir}" "${output}")
        string(APPEND shader_header "#include <${relative}>\n")
    endforeach()
    file(GENERATE OUTPUT "${output_dir}/centui_shaders.h" CONTENT "${shader_header}")

    target_sources(${target} PRIVATE ${vertex_shaders} ${fragment_shaders} ${shader_outputs}
        "${output_dir}/centui_shaders.h")
    target_include_directories(${target} PRIVATE "${output_dir}" "${bgfx_cmake_SOURCE_DIR}/bx/include")
    target_link_libraries(${target} PRIVATE bgfx)
endfunction()
