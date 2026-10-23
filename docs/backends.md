---
layout: default
title: Backends and Windows
nav_order: 5
---

# Backends and Windows

CentUI keeps window input and drawing separate. A platform reports the window size, translates native events, and provides services such as clipboard access. A renderer draws the document's rectangles, images, and prepared text. `centui::ui` joins them for one document.

## Supported combinations

| Platform | SDL 2D renderer | SFML renderer | bgfx renderer |
| --- | --- | --- | --- |
| SDL | [hud_sdl](https://github.com/swordfatih/centui/blob/main/examples/hud_sdl/main.cpp) | — | [getting_started](https://github.com/swordfatih/centui/blob/main/examples/getting_started/main.cpp) |
| SFML | — | [hud_sfml](https://github.com/swordfatih/centui/blob/main/examples/hud_sfml/main.cpp) | — |
| GLFW | — | — | [hud_glfw](https://github.com/swordfatih/centui/blob/main/examples/hud_glfw/main.cpp) |

Each linked example shows a supported pairing. A dash means CentUI does not provide that combination.

`CENTUI_BUILD_SDL=ON` builds both the SDL platform and SDL renderer. `CENTUI_BUILD_SFML=ON` likewise builds both SFML adapters.

For a window owned by CentUI's SDL adapter and drawn through bgfx:

```cpp
centui::sdl::platform platform{{.title = "My app", .width = 800, .height = 600}};
centui::bgfx::backend renderer{platform};
centui::run(platform, renderer, document);
```

Build the platform before the renderer so the renderer releases its graphics resources first. The [getting_started example](https://github.com/swordfatih/centui/blob/main/examples/getting_started/main.cpp) contains the full program.

For SDL's own 2D renderer, include `<centui/backends/sdl.hpp>` and construct `centui::sdl::backend renderer{platform}`. It supports the same `centui::run(platform, renderer, document)` loop. You can also borrow an existing `SDL_Renderer` with `centui::sdl::backend renderer{native_renderer, platform}`. Draw your SDL scene before `ui.render(document)` to put the interface on top. The [hud_sdl example](https://github.com/swordfatih/centui/blob/main/examples/hud_sdl/main.cpp) shows this setup.

GLFW can host the same bgfx backend: include `<centui/platforms/glfw.hpp>`, then construct `centui::glfw::platform window{options}` and `centui::bgfx::backend renderer{window}`. Use `centui::glfw::platform window{existing_glfw_window}` to borrow a window. The adapter chains callbacks already installed on that window without using GLFW's user pointer; install new callbacks before constructing the adapter. If your application calls `glfwPollEvents()` itself, pass `window.take_events()` to `ui.process_events()`. GLFW supplies committed text but does not expose the composition and candidate-window services used by CentUI's richer text input. See [hud_glfw](https://github.com/swordfatih/centui/blob/main/examples/hud_glfw/main.cpp).

## Draw over an existing SFML window

```cpp
sf::RenderWindow window{sf::VideoMode{{800, 600}}, "My app"};
centui::sfml::platform platform{window};
centui::sfml::backend renderer{platform};
centui::ui ui{platform, renderer};

while(window.isOpen())
{
    while(const auto event = window.pollEvent())
    {
        ui.process_event(document, *event);
        if(event->is<sf::Event::Closed>())
        {
            window.close();
        }
    }
    if(!window.isOpen())
    {
        break;
    }
    window.clear();
    // Draw your scene here.
    ui.render(document);
    window.display();
}
```

The SFML adapters borrow the window; your application still owns its event loop and frame. Draw the scene before `ui.render()` so the interface appears on top. The [hud_sfml example](https://github.com/swordfatih/centui/blob/main/examples/hud_sfml/main.cpp) draws the green circle from SFML's introductory program under a CentUI panel.

## Another renderer or host

Implement `centui::renderer::viewport()` and `draw(const centui::draw_list&)` to use another graphics API. The document prepares commands; your backend sends them to the graphics device. [custom_backend](https://github.com/swordfatih/centui/blob/main/examples/custom_backend/main.cpp) logs the commands without opening a window.

A custom `centui::platform` supplies native event translation, a viewport, and host services. Coordinates in widget layout and input are logical units; the viewport's scale relates them to framebuffer pixels. `centui::ui` can also work with just a renderer when your application supplies translated input itself.
