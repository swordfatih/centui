---
layout: default
title: "2. Widgets and styles"
nav_order: 3
---

# Widgets and styles

An interface is a tree. Containers arrange children; controls display content and respond to input. Keep related widgets together, then name the appearances you want to reuse.

## Make a small component

A style struct declares only the properties it changes:

```cpp
struct card
{
    centui::edges padding = centui::edges::all(centui::px(24));
    centui::length gap = centui::px(12);
    float corner_radius = 12;
    centui::states_color background_color = centui::states_color::all({30, 41, 59, 255});
};

struct heading
{
    float font_size = 28;
};

auto welcome = centui::container<card>{
    centui::text<heading>{"Your workspace"},
    centui::text{"Pick something to work on."},
};
```

Use the same types across the app. `container<card, wide>` applies `card` first, then the properties declared by `wide`. Font requests, font size, and text color inherit through the tree.

## Arrange a row

Containers stack children vertically by default. A row changes the direction; a flexible child takes the remaining space:

```cpp
struct row
{
    centui::direction flex_direction = centui::direction::row;
    centui::alignment align_items = centui::alignment::center;
    centui::length gap = centui::px(12);
};

struct fill
{
    centui::flex flex = centui::flex::all(1, 1, centui::px(0));
};

auto toolbar = centui::container<row>{
    centui::text<fill>{"Workspace"},
    centui::button{"Help"},
};
```

`flex::all(grow, shrink, basis)` describes how a child shares space. Here its starting size is zero and it can both grow and shrink. Put the children in the order you want to read and navigate them.

## Choose sizes that adapt

| Use | For |
| --- | --- |
| `px(24)` | Padding, spacing, and a specific logical size. |
| `percent(100)` | Filling the containing space. |
| `automatic()` | Sizing from content and layout. |
| `min_width`, `max_width` | Keeping a flexible panel useful at different sizes. |
| `flex` | Sharing the space left beside other children. |

Lengths are logical units. The platform handles display scale. `padding` is inside a widget, `margin` is outside it, and `gap` separates its children.

For a page that may grow taller than its window, put its content in `scroll`. Give the scroll viewport a bounded size or a flexible place in a bounded parent. The [task list](https://github.com/swordfatih/centui/blob/main/tests/examples/task_list/main.cpp) shows this arrangement.

## Give controls feedback

Set state colors in a named style:

```cpp
struct action
{
    centui::edges padding = centui::edges::all(centui::px(12));
    centui::states_color background_color{
        .normal = {37, 99, 235, 255},
        .hover = {59, 130, 246, 255},
        .pressed = {29, 78, 216, 255},
    };
};
```

Use `button<action>` for a text action. Use `pressable<action>` when the clickable content is a whole card or another subtree. Standard controls already handle pointer and keyboard activation.

## Change a theme while the app runs

Keep initial appearance in named styles. Use document patches when the user changes a theme:

```cpp
document.patch_style<card>({
    .background_color = centui::states_color::all({241, 245, 249, 255}),
});
```

Existing matching widgets update without rebuilding the tree. Multiple style types still apply in order, and a widget's local overrides take precedence. The [dashboard](https://github.com/swordfatih/centui/blob/main/tests/examples/dashboard/main.cpp) is a complete theme example.

[← Your first app](getting-started.md) · [Next: state and interaction →](interaction.md)
