---
layout: default
title: "4. Text and fonts"
nav_order: 5
---

# Text and fonts

Text uses system fonts by default. Start there, then choose a family or register a font when your application needs a particular appearance.

## Name a text style

```cpp
struct article
{
    float font_size = 18;
    centypo::font font{"serif"};
    centypo::text_options paragraph{
        .wrap = centypo::text_wrap::word_grapheme,
        .line_spacing = 4,
    };
};

auto body = centui::text<article>{
    "A longer paragraph wraps to the width given to it by the surrounding layout."
};
```

The generic families `sans-serif`, `serif`, and `monospace` resolve through system font discovery. Set text styles on a container when its descendants should share them. A width constraint gives wrapping something to wrap against.

Pass family names in preference order, such as `centypo::font{"Arial", "Inter"}`.

## Use an application font

Register your file before the first frame:

```cpp
document.register_font(std::filesystem::path{"assets/MyFont.ttf"});
document.set_style({.font = centypo::font{"My Font"}});
```

Use the family name stored inside the font file, which can differ from its filename. The path belongs to your application: a relative path is resolved from its working directory. Registering a byte span is also supported. Font changes go through the document so affected widgets update.

## Keep a preview short

A paragraph style can limit a label without changing the source text:

```cpp
struct preview
{
    centypo::text_options paragraph{
        .max_lines = 2,
        .ellipsis = true,
    };
};
```

Place `text<preview>` in a constrained-width panel. Its display follows the space available to it.

## Let the input widget handle editing

```cpp
auto notes = centui::input{"Write a note…", {.multiline = true}};
```

Inputs handle selection, caret movement, undo, and text composition through the host adapter. Use `on_change` to update your model and `on_submit` for a single-line action. Host support varies; GLFW supplies committed text but has no composition API.

Strings are literal UTF-8, not markup. Invalid UTF-8 and embedded NUL are rejected. When you use selection APIs directly, offsets refer to UTF-8 bytes rather than character counts.

## Use text outside a widget

CentUI's text engine is [Centypo](https://github.com/swordfatih/centui/blob/main/src/centypo/README.md). `document.typography()` exposes layout and hit testing for custom drawing. For an independent text engine, link `centypo::centypo` directly.

[← State and interaction](interaction.md) · [Next: an existing application →](backends.md)
