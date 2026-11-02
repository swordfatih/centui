---
layout: default
title: "1. Your first app"
nav_order: 2
---

# Your first app

Build a counter: a label, a button, and a little state. You need CMake 3.28+ and a compiler with C++20 support.

## Add CentUI to a project

Create a directory containing `CMakeLists.txt` and `main.cpp`. This setup selects SDL and bgfx and leaves repository examples and tests out of your application build:

```cmake
cmake_minimum_required(VERSION 3.28)
project(counter LANGUAGES C CXX)

include(FetchContent)
set(CENTUI_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CENTUI_BUILD_TESTS OFF CACHE BOOL "")
set(CENTUI_BUILD_SFML OFF CACHE BOOL "")
set(CENTUI_BUILD_GLFW OFF CACHE BOOL "")
set(CENTUI_BUILD_SDL ON CACHE BOOL "")
set(CENTUI_BUILD_BGFX ON CACHE BOOL "")

FetchContent_Declare(centui
    GIT_REPOSITORY https://github.com/swordfatih/centui.git
    GIT_TAG main
)
FetchContent_MakeAvailable(centui)

add_executable(counter main.cpp)
target_link_libraries(counter PRIVATE centui::platform::sdl centui::backend::bgfx)
```

Pin `GIT_TAG` to a commit when you adopt the library. For system dependencies or an existing checkout, see [build options](build.md).

## Write the app

Put this in `main.cpp`:

```cpp
#include <string>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

struct screen
{
    centui::length width = centui::percent(100);
    centui::length height = centui::percent(100);
    centui::length gap = centui::px(20);
    centui::alignment align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    float font_size = 24;
};

struct action
{
    centui::edges padding = centui::edges::all(centui::px(12));
    centui::states_color background_color = centui::states_color::all({37, 99, 235, 255});
};

int main()
{
    centui::sdl::platform window{{.title = "Counter", .width = 640, .height = 400}};
    centui::bgfx::backend renderer{window};

    int count = 0;
    auto label = centui::text{"Clicked 0 times"};

    auto root = centui::container<screen>{
        label,
        centui::button<action>{
            "Click me",
            {
                .on_click = [&count, label]
                {
                    label->set_text("Clicked " + std::to_string(++count) + " times");
                },
            }
        },
    };

    centui::document document{root, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{"assets/fonts/Inter-Regular.ttf"});
    document.set_style({.font = centypo::font{"Inter"}});

    centui::ui ui{window, renderer};

    while(window.poll_events())
    {
        ui.frame(document);
    }
}
```

The tree is built once. `label` is a handle to the widget inside it, so the callback changes the existing label. The `screen` and `action` structs describe appearance; they do not own widgets or state.

`document` holds the tree. The platform supplies input and window size; the renderer draws it. Keep them alive while the loop runs, with the window constructed before the renderer.

## Build and click

```sh
cmake -S . -B build
cmake --build build --target counter
```

Copy `Inter-Regular.ttf` and its license from the repository's `tests/assets/fonts` into `assets/fonts` under the directory you run the app from. Run `counter` from that directory. Multi-configuration generators place the executable under a directory such as `Debug`. The app explicitly registers Inter, so it uses the same font regardless of installed system fonts.

For the slightly more styled repository version, read [getting_started/main.cpp](https://github.com/swordfatih/centui/blob/main/tests/examples/getting_started/main.cpp).

[← Documentation](index.md) · [Next: widgets and styles →](widgets-and-styles.md)
