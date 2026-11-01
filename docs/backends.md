---
layout: default
title: "5. An existing application"
nav_order: 6
---

# An existing application

Your app can keep its window, scene, and event loop. CentUI needs a platform for input and window services, and a renderer to draw the interface.

## Pick a pairing

| Platform | Renderer | Enable |
| --- | --- | --- |
| SDL | SDL 2D | `CENTUI_BUILD_SDL` |
| SDL | bgfx | `CENTUI_BUILD_SDL`, `CENTUI_BUILD_BGFX` |
| SFML | SFML | `CENTUI_BUILD_SFML` |
| GLFW | bgfx | `CENTUI_BUILD_GLFW`, `CENTUI_BUILD_BGFX` |

The [build reference](build.md#choose-your-adapters) lists their CMake targets.

## Let CentUI draw a whole window

This is the arrangement used in the first app:

```cpp
centui::sdl::platform window{{.title = "My app", .width = 800, .height = 600}};
centui::bgfx::backend renderer{window};

centui::ui ui{window, renderer};

while(window.poll_events())
{
    ui.frame(document);
}
```

`frame()` processes the polled input, advances time, and renders. Create the window before the renderer so it outlives the renderer's graphics resources.

For SDL's 2D renderer, use `centui::sdl::backend`. For GLFW with bgfx, use `centui::glfw::platform` in place of the SDL platform.

## Draw over an SFML scene

With an existing `sf::RenderWindow window` and a CentUI `document`, wrap the window and keep presentation in your loop:

```cpp
centui::sfml::platform platform{window};
centui::sfml::backend renderer{platform};

centui::ui ui{platform, renderer};

while(window.isOpen())
{
    while(const auto event = window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            window.close();
            break;
        }

        ui.process_event(document, *event);
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

The [scene-overlay example](https://github.com/swordfatih/centui/blob/main/tests/examples/overlay/main.cpp) includes resizing and controls that change a circle in the scene. The adapters borrow the window; keep it alive until they are destroyed.

## Own the frame timing

`ui.render(document)` draws without advancing animation. In a loop like the one above, call `document.advance(elapsed)` with your elapsed `centui::frame_duration` when using animated widgets. If CentUI owns the whole frame, `ui.frame(document, elapsed)` accepts the same timing directly.

SDL and GLFW adapters can also borrow existing windows. When your app already calls `glfwPollEvents()`, pass the adapter's `take_events()` result to `ui.process_events()` instead of polling a second time. Install GLFW callbacks before wrapping the window so the adapter can chain them.

For another graphics API, implement the small [renderer interface](https://github.com/swordfatih/centui/blob/main/include/centui/render.hpp). Its input is a prepared draw list. Widget drawing stays independent of the graphics API.

[← Text and fonts](text.md) · [Next: custom widgets →](extending.md)
