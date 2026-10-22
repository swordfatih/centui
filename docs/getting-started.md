---
layout: default
title: Getting Started
nav_order: 2
---

# Getting Started

CentUI uses C++26 modules and static reflection. You need CMake 4.3 or newer, Ninja, and a compiler and standard library that support the features used by `<meta>` and `-freflection`. The build downloads Yoga, FreeType, HarfBuzz, and the enabled window and rendering libraries.

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
