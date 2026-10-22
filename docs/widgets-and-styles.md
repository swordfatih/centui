---
layout: default
title: Widgets and Styles
nav_order: 3
---

# Widgets and Styles

CentUI widgets are ordinary objects connected by parent and child relationships. The convenient `text`, `button`, `container`, `input`, and other handles make a tree readable at the call site:

```cpp
auto status = centui::text{"Waiting"};
auto root = centui::container{
    status,
    centui::button{"Go", {.on_click = [status]() mutable
    {
        status.set_text("Done");
    }}},
};
```

`button` is a text control. `pressable` handles the same focus, hover, pointer, and keyboard behavior around any content:

```cpp
auto tile = centui::pressable<tile_style>{
    centui::container{
        centui::text{"Project"},
        centui::text{"Open details"},
    },
    {.on_click = open_project},
};
```

Other controls are `toggle`, `range`, `selection`, `scroll`, `popup`, and single-line or multiline `input`. Their option structs carry initial values and callbacks. See [gui_static](https://github.com/swordfatih/centui/blob/main/examples/gui_static/main.cpp) for all of them together. You can also build the same tree with `make_widget<T>()` and `append()`; [gui_dynamic](https://github.com/swordfatih/centui/blob/main/examples/gui_dynamic/main.cpp) uses that approach.

## Give widgets a style

A style struct names only the properties you want to change:

```cpp
struct blue_button
{
    centui::length height = centui::px(44);
    centui::edges padding = centui::edges::all(centui::px(12));
    centui::states_color background_color =
        centui::states_color::all(centui::color::from_hex(0x2563EB));
};

auto save = centui::button<blue_button>{"Save"};
```

You can combine style structs, use a `centui::style` value at runtime, or change a mounted widget with `patch_style({.height = centui::px(52)})`. The layout system handles width, height, padding, margins, gaps, alignment, and direction. State colors can vary for normal, hover, press, focus, and disabled states. Text inherits a font from its ancestors or the document's font service.

## Keep a handle when you need it

Copies of a handle refer to the same widget. A parent owns its children, and a document mounts one root. Keep a named handle for a widget you plan to update. You can remove or append children through widget methods or the document; edits requested inside callbacks settle before the next input event.

If a callback must refer to its own widget or an ancestor, capture `weak()` and check `lock()` before using it. A strong capture there would keep both sides alive. For example, the [Studio popup](https://github.com/swordfatih/centui/blob/main/examples/gui_static/main.cpp) closes through a weak handle.
