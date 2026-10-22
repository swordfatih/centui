---
layout: default
title: Getting Started
nav_order: 2
---

# Getting Started

CentUI uses C++26 modules and static reflection. You need CMake 4.3 or newer, Ninja, and a compiler and standard library that support the features used by `<meta>` and `-freflection`. GCC builds require 16.2 or newer to avoid the modules and reflection declaration merging bug [PR124582](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124582). The build downloads Yoga, FreeType, HarfBuzz, and the enabled window and rendering libraries.

## Use CentUI in your own project

Create a `CMakeLists.txt` next to your application's `main.cpp`:

```cmake
cmake_minimum_required(VERSION 4.3)
project(my_app LANGUAGES C CXX)

include(FetchContent)

# Enable the integrations before fetching CentUI.
set(CENTUI_BUILD_SDL ON CACHE BOOL "")
set(CENTUI_BUILD_BGFX ON CACHE BOOL "")

FetchContent_Declare(centui
    GIT_REPOSITORY https://github.com/swordfatih/centui.git
    GIT_TAG main
)
FetchContent_MakeAvailable(centui)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE centui::centui centui::sdl centui::bgfx)
```

Use the [README's Hello world](../README.md#hello-world) as `main.cpp`. Configure and build from your application's directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target my_app
```

Use the same supported compiler for your application and CentUI. The linked targets propagate the C++26 requirement and reflection flags; CMake scans module imports and builds their dependencies. Replace `GIT_TAG main` with a specific commit to pin your dependency.

The Hello world program loads `assets/fonts/Inter-Regular.ttf` relative to its working directory. Supply that font (or change the path to your own font) and launch the program from the directory containing `assets/`. FetchContent does not copy font assets into your application.

Choose the targets for your integration:

| CMake option | Targets |
| --- | --- |
| Core only (default) | `centui::centui` |
| `CENTUI_BUILD_SDL=ON` | `centui::sdl`, `centui::sdl_renderer` |
| `CENTUI_BUILD_BGFX=ON` | `centui::bgfx` |
| `CENTUI_BUILD_SFML=ON` | `centui::sfml`, `centui::sfml_platform` |
| `CENTUI_BUILD_GLFW=ON` | `centui::glfw` |

Examples and tests are off by default. Native dependencies are fetched automatically. Platform packages may also be required for your selected integration; see [Backends and Windows](backends.md).

## Build the first example

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCENTUI_BUILD_EXAMPLES=ON -DCENTUI_BUILD_SDL=ON -DCENTUI_BUILD_BGFX=ON
cmake --build build --target centui_example_getting_started
```

Run `centui_example_getting_started` from `build/examples/getting_started/`. The example copies its font beside the executable. To build the SFML overlay too, configure with `-DCENTUI_BUILD_SFML=ON` and build `centui_example_hud_sfml`.

All optional platforms, renderers, examples, and tests are off by default. Enable only the pieces your application uses.

## Meet the pieces

```cpp
auto label = centui::text{"Ready"};
auto root = centui::container{
    label,
    centui::button{"Update", {.on_click = [label]() mutable
    {
        label.set_text("Updated");
    }}},
};

centui::document document{root};
document.set_font(centui::font{"assets/fonts/Inter-Regular.ttf"});
```

`container` arranges its children. `text` paints the label. `button` responds to pointer and keyboard activation. The label variable is a handle to the same widget inside the tree, so the callback can update it. `document` owns the root while it is mounted and prepares layout, input, and drawing. Text needs a font; this path assumes you launch from the repository root.

An SDL window and bgfx renderer can run the document with `centui::run(platform, renderer, document)`. For your own event loop, use `centui::ui` to forward events and draw the prepared interface. [Backends and Windows](backends.md) shows both arrangements.

When you want more than text, put arbitrary children inside a `pressable`. The [widgets guide](widgets-and-styles.md) shows that pattern and explains styling.
