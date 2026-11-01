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

The current repository defaults all four adapter options, `CENTUI_BUILD_EXAMPLES`, and `CENTUI_BUILD_TESTS` to `ON`. When consuming the library, turn off the parts you do not use before calling `FetchContent_MakeAvailable` or `add_subdirectory`.

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

The library does not bundle fonts. If your target environment lacks suitable system fonts, [register application font data](text.md#use-an-application-font).

## Work on CentUI

With `CENTUI_BUILD_TESTS=ON`, build the test executables and run them through CTest:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Integration tests use the enabled adapters and may open windows. `ctest -L integration` selects them; `ctest -LE integration` excludes them. On a headless Linux machine, the graphics tests need a virtual display such as Xvfb.

See [CONTRIBUTING.md](https://github.com/swordfatih/centui/blob/main/CONTRIBUTING.md) for code conventions.

[← Reference](reference.md) · [Performance →](performance.md)
