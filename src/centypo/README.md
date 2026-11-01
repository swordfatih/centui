# Centypo

A C++20 text engine for layout, hit testing, editing, and glyph rasterization. It can be used independently of CentUI and does not require a window or graphics API.

## Lay out some text

```cpp
#include <centypo/centypo.hpp>

centypo::engine typography;

auto text = typography.layout("Hello, world", {.size = 18, .width = 320});
auto bounds = text.bounds();
auto position = text.hit_test({42, 10});
auto caret = text.caret_bounds(position);
auto selection = text.selection_bounds(0, 5);
```

System fonts are discovered automatically. Use `register_font` for an application font. Strings are literal UTF-8; positions use byte offsets and visual affinity.

Retain a `centypo::layout` value to reuse the result. It keeps the fonts it needs alive. Use each engine on one application thread.

## Prepare it for drawing

```cpp
auto raster = typography.rasterize(text, 2);
```

The scale controls pixels per logical unit. Rasterization supplies glyph images while preserving the layout's logical geometry; your renderer draws, tints, and clips them.

`centypo::editor` provides text, selection, composition, and undo/redo without a widget or clipboard dependency.

## Add it to your project

Add this repository through FetchContent or `add_subdirectory`, then link `centypo::centypo`. Public headers are under `include/centypo`. Its text dependencies are fetched automatically. Linux system font discovery requires Fontconfig development headers; `CENTYPO_SYSTEM_FONTS=OFF` selects explicit font registration.

For use inside CentUI, start with [Text and fonts](../../docs/text.md). Exact types and operations are documented in the [public headers](../../include/centypo).
