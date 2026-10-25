include_guard(GLOBAL)

if(NOT EMSCRIPTEN)
    return()
endif()

# CentUI uses exceptions for validation and dispatch cleanup. Keep dependencies and
# consumers on the same exception model without imposing application output settings.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
target_compile_options(centui PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
target_link_options(centui PUBLIC -fexceptions)
