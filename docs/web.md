---
layout: default
title: Web
nav_order: 7
---

# Web with Emscripten

The web path uses the SDL platform adapter and the bgfx backend with WebGL 2.
The browser example owns its frame callback and object lifetime; CentUI processes
events, advances widgets, and renders through the same `ui.frame(document)` API
used on desktop. A browser with WebAssembly and WebGL 2 is required.

## Native shader compiler

Shaders must be compiled by a host executable, not a WebAssembly program. Build
`shaderc` with your normal desktop compiler in a separate directory. Using
CentUI's dependency checkout keeps the shader compiler and bgfx revision aligned:

```sh
cmake -S . -B build/host -G Ninja -DCENTUI_BUILD_BGFX=ON
cmake --build build/host --target shaderc --config Release
```

With Ninja, the executable is normally
`build/host/_deps/bgfx_cmake-build/cmake/bgfx/shaderc` (`shaderc.exe` on Windows).
Multi-configuration generators add a configuration directory such as `Release`.
You can also supply an existing host `shaderc` built from the same pinned bgfx
revision. Web builds do not compile bgfx's desktop tools.

## Configure the browser example

Install and activate the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html),
then configure a separate build directory from its environment. Replace the
shader compiler path below with the absolute path to your host executable:

```sh
emcmake cmake -S . -B build/web -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCENTUI_BUILD_SDL=ON \
  -DCENTUI_BUILD_BGFX=ON \
  -DCENTUI_BUILD_EXAMPLES=ON \
  -DCENTUI_BGFX_SHADERC=/absolute/path/to/shaderc
cmake --build build/web --target centui_example_web
```

Only the browser examples are added when configuring examples with Emscripten.
The normal desktop examples retain their application-owned desktop loops.
Leave `CENTUI_BUILD_TESTS` off for this example: the current native test suite
is not a browser test harness.

Serve the output over HTTP:

```sh
python -m http.server 8000 --directory build/web/examples/web
```

Open `http://localhost:8000/centui_example_web.html`. Deploy its HTML, JavaScript,
WebAssembly, and asset data files together. Opening the HTML directly through
`file://` is not sufficient for loading the application and assets.

## Pulse dashboard

`examples/pulse_web` copies the desktop Pulse dashboard, preserving its widget
tree, styles, charts, and callbacks. Its window, renderer, document, and UI remain
alive after `main()`, and a browser animation callback drives `ui.frame()`.
It uses the same HTML shell as the smaller web example. Each example's
`CMakeLists.txt` defines its own font packaging and web link settings.
The desktop `examples/pulse` is unchanged.

With the same Emscripten configuration, build and serve it with:

```sh
cmake --build build/web --target centui_example_pulse_web --parallel 4
python -m http.server 8000 --directory build/web/examples/pulse_web
```

Open `http://localhost:8000/centui_example_pulse_web.html`. Use a desktop-sized
browser window to view the full dashboard.

## Integration boundaries

The SDL adapter reads the canvas selector from SDL's window properties; it does
not hard-code a DOM element. SDL defaults to `#canvas`. Applications using a
different canvas can set `SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR` before constructing
the platform. Keep the canvas in the DOM while the window and renderer exist.

The bgfx adapter uses that selector to create an OpenGL ES/WebGL 2 context. Web
builds embed ESSL 3.00 shaders and run bgfx without a rendering thread. SDL supplies
browser input, window size, and display scaling through the existing adapter.

`examples/web` supplies the HTML shell, focusable canvas, full-window CSS, font
preloading, memory-growth settings, and a browser animation callback. These are
application choices and are not imposed on consumers by the adapter targets.
The callback's objects have static storage duration so they survive `main()`.

CentUI's CMake setup enables C++ exception handling for its sources, fetched
dependencies, and consumers, because validation and event-dispatch cleanup use
exceptions. Your application should use a matching exception model for other
libraries that exchange exceptions with it.

## Browser checks

After building, check that the page renders text and controls without console
errors, the button updates the counter, and the input accepts keyboard editing.
Resize the browser and check layout, pointer alignment, and crisp text at different
display scales. Test touch input on mobile browsers separately. The desktop HUD
examples continue to manage their own frame and draw through `ui.render()`.
