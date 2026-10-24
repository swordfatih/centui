# CentUI

CentUI is a small C++20 library for building interfaces from ordinary widgets. Compose a tree, give it a style, and connect callbacks to your application state. SDL/bgfx and SFML integrations let you draw in a CentUI window or over one you already own.

## Showcase

<p align="center">
  <img src="docs/assets/showcase.png" alt="CentUI Studio showcase" width="600">
  <br>
  <em><a href="examples/pulse/main.cpp">pulse example</a> (SDL platform, bgfx rendering backend)</em>
</p>

## Hello world

```cpp
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>
#include <centui/backends/bgfx.hpp>

int main()
{
    centui::sdl::platform window{{.title = "Hello, CentUI", .width = 640, .height = 480}};
    centui::bgfx::backend renderer{window};

    auto message = centui::text{"Hello, world!"};
    auto root = centui::container{
        message,
        centui::button{"Click me", {.on_click = [message]() mutable
        {
            message.set_text("You clicked the button!");
        }}},
    };

    centui::document document{root};
    document.set_font(centui::font{"assets/fonts/Inter-Regular.ttf"});
    centui::run(window, renderer, document);
}
```

Run this from the repository root so the font path resolves. For a styled counter you can build immediately, see [getting_started](examples/getting_started/main.cpp).

## Installation

Requires CMake 3.28+ and a compiler and standard library with C++20 support. Add CentUI with CMake FetchContent and enable the integrations your application needs:

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES C CXX)

include(FetchContent)
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

Link `centui::centui` for the core library and the targets for your chosen integrations. CMake fetches the dependencies and propagates the required include paths and C++20 support. Pin `GIT_TAG` to a commit for reproducible builds. See [window and renderer integration](docs/backends.md) for the available backends.

## Build the examples

From a checkout of this repository, build the SDL/bgfx example with Ninja:

```sh
cmake -S . -B build -G Ninja -DCENTUI_BUILD_EXAMPLES=ON -DCENTUI_BUILD_SDL=ON -DCENTUI_BUILD_BGFX=ON
cmake --build build --target centui_example_getting_started
```

The executable is in `build/examples/getting_started/`. Other examples include a [complete interface built as a nested tree](examples/gui_static/main.cpp), [the same interface built with helpers](examples/gui_dynamic/main.cpp), and SDL, SFML, and GLFW overlays in [hud_sdl](examples/hud_sdl/main.cpp), [hud_sfml](examples/hud_sfml/main.cpp), and [hud_glfw](examples/hud_glfw/main.cpp).

The [custom primitive example](examples/custom_primitive/main.cpp) demonstrates
an animated gradient widget controlled by a slider, with retained vector paths,
and rounded corners. Build `centui_example_custom_primitive` with SDL
and bgfx enabled.
The [Pulse dashboard](examples/pulse/main.cpp) demonstrates a vector chart with
a gradient area fill and proportional traffic bars.

CentUI is still evolving. Start with the [getting started guide](docs/getting-started.md), then explore [widgets and styles](docs/widgets-and-styles.md), [custom widgets](docs/extending.md), or [window and renderer integration](docs/backends.md).
