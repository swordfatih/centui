---
layout: default
title: Build and integration
parent: Reference
nav_order: 1
---

# Build and integration

Use CMake 3.28+ and a C++20 compiler and standard library. Dependencies are fetched during configuration. The [first-app guide](getting-started.md#add-centui-to-a-project) contains a complete FetchContent project.

## Choose your adapters

Link `centui::centui` for the core library. Adapter targets link the core transitively:

| Option | Targets |
| --- | --- |
| `CENTUI_BUILD_SDL` | `centui::platform::sdl`, `centui::backend::sdl` |
| `CENTUI_BUILD_BGFX` | `centui::backend::bgfx` |
| `CENTUI_BUILD_SFML` | `centui::platform::sfml`, `centui::backend::sfml` |
| `CENTUI_BUILD_GLFW` | `centui::platform::glfw` |

All four adapter options, `CENTUI_BUILD_EXAMPLES`, and `CENTUI_BUILD_TESTS` default to `OFF`. Enable the adapters you need before calling `FetchContent_MakeAvailable` or `add_subdirectory`. When building examples from a checkout, also enable `CENTUI_BUILD_EXAMPLES`; the getting-started counter requires both SDL and bgfx.

`CENTYPO_SYSTEM_FONTS` also defaults to `ON`. Turn it off only when you will register your own fonts.

## Use a local checkout

For a vendored directory or Git submodule:

```cmake
add_subdirectory(external/centui)
target_link_libraries(my_app PRIVATE centui::platform::sdl centui::backend::bgfx)
```

Set the same options before `add_subdirectory`. FetchContent can also use `SOURCE_DIR` instead of a Git repository. An installed `find_package(centui)` package is not provided.

## System dependencies

The compiler, CMake, and your chosen generator must be installed. On Linux, system font discovery needs Fontconfig development headers. SDL, SFML, GLFW, and bgfx may need native window-system and graphics development packages for the selected platform. The repository's [CI workflow](https://github.com/swordfatih/centui/blob/main/.github/workflows/ci.yml) shows the package lists used for its desktop builds.

The library does not bundle fonts. Repository tests and examples explicitly register Inter from `tests/assets/fonts` rather than relying on installed fonts. Applications can [register their own font data](text.md#use-an-application-font).

## Shader compiler for cross-compilation

Shader compilation uses bgfx's `bgfx_compile_shaders()` on every target. A normal desktop build builds `shaderc` alongside bgfx. To use an existing compiler, set `CENTUI_BGFX_SHADERC` to the absolute path of a host executable built from the same bgfx revision:

```sh
cmake -S . -B build/host -DCENTUI_BUILD_BGFX=ON
cmake --build build/host --target shaderc
```

Pass the resulting executable to the target build with `-DCENTUI_BGFX_SHADERC=/absolute/path/to/shaderc`. Its location depends on the generator and build configuration. Cross-compiling requires this setting or an existing executable target named `bgfx::shaderc`. The executable runs on the build machine; bgfx's shader helper selects shader formats for the target platform. Supplying a host compiler disables building bgfx tools for the target.

This applies to mobile and other cross-compiled targets without a separate CentUI shader-compilation path. It does not enable runtime support for a platform that the chosen adapters do not yet implement.

## Work on CentUI

With `CENTUI_BUILD_TESTS=ON`, build the test executables and run them through CTest:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Integration tests use the enabled adapters and may open windows. `ctest -L integration` selects them; `ctest -LE integration` excludes them. On a headless Linux machine, the graphics tests need a virtual display such as Xvfb.

See [CONTRIBUTING.md](https://github.com/swordfatih/centui/blob/main/CONTRIBUTING.md) for code conventions.

[← Reference](reference.md) · [Performance →](performance.md)
