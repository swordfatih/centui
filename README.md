# CentUI

CentUI is a small C++26 library for building interfaces from ordinary widgets. Compose a tree, give it a style, and connect callbacks to your application state. SDL/bgfx and SFML integrations let you draw in a CentUI window or over one you already own.

<p align="center">
  <img src="docs/assets/showcase.png" alt="CentUI Studio showcase" width="600">
</p>

```cpp
#include <filesystem>
#include <initializer_list>

import centui;
import centui.sdl;
import centui.bgfx;

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

## Try it

You need CMake 4.3+, Ninja, and a C++26 compiler and standard library with static reflection support (`-freflection`). CMake fetches the native dependencies during configuration.

```sh
cmake -S . -B build -G Ninja -DCENTUI_BUILD_EXAMPLES=ON -DCENTUI_BUILD_SDL=ON -DCENTUI_BUILD_BGFX=ON
cmake --build build --target centui_example_getting_started
```

The executable is in `build/examples/getting_started/`. Other examples include a [complete interface built as a nested tree](examples/gui_static/main.cpp), [the same interface built with helpers](examples/gui_dynamic/main.cpp), and SDL, SFML, and GLFW overlays in [hud_sdl](examples/hud_sdl/main.cpp), [hud_sfml](examples/hud_sfml/main.cpp), and [hud_glfw](examples/hud_glfw/main.cpp).

CentUI is still evolving. Start with the [getting started guide](docs/getting-started.md), then explore [widgets and styles](docs/widgets-and-styles.md), [custom widgets](docs/extending.md), or [window and renderer integration](docs/backends.md).
