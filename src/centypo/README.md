# Centypo

A C++20 text engine. Fonts, shaping, layout, editing and glyph rasterization work
without a UI toolkit or GPU renderer.

```cpp
#include <centypo/centypo.hpp>

centypo::engine typography;
auto text = typography.layout("Hello, world", {.size = 18, .width = 320});

auto bounds = text.bounds();
auto cursor = text.hit_test({42, 10});
auto caret = text.caret_bounds(cursor);
auto selection = text.selection_bounds(0, 5);
```

`layout` is an immutable value. Copies share storage internally; retaining a
layout keeps its fonts alive after cache eviction, font refresh or destruction
of the engine. Default-constructed and moved-from layouts are empty values.
The engine is movable and owns its font collection and bounded caches. Use each
engine on one application thread.

Text is literal UTF-8. Invalid encoding and embedded NUL are rejected. Positions
use byte offsets and visual affinity. Layouts expose read-only glyph, line and
caret spans; those views remain valid while the corresponding layout is retained.

System fonts are enabled by default. Register application fonts from a path or
byte span; registration snapshots the data. No fonts are embedded in the library.

```cpp
centypo::engine typography({.system_fonts = false});
typography.register_font(std::filesystem::path{"application.ttf"});
auto text = typography.layout("Explicit font");
auto raster = typography.rasterize(text, 2); // Two pixels per logical unit.
```

Logical layout allocates no glyph images. Rasterization preserves logical
geometry and provides grayscale or straight-RGBA bitmap resources through each
glyph. Drawing, tint, clipping and host input integration belong to the consumer.
Repeated rasterization of the same layout and scale reuses prepared storage.
Engine-prepared aligned and rasterized results share the layout cache budget; raster admission
also respects the image budget and reserves space for retained pixels.
`layout::aligned()` creates an independently owned copy outside engine caches.

`centypo::editor` owns committed text, directional selection, provisional
composition and undo/redo. It has no widget, window or clipboard dependency.

## Build

Centypo is built as part of this repository through the `centypo::centypo` target.

```sh
cmake -S . -B build
cmake --build build
```

Public headers live in `include/centypo`, alongside `include/centui`.
Consumers can add this repository and link `centypo::centypo` directly.
FreeType, HarfBuzz, SheenBidi and libunibreak are private
dependencies, fetched at pinned revisions. Standard FetchContent source overrides
allow vendored dependencies. Linux discovery requires Fontconfig development
headers; `CENTYPO_SYSTEM_FONTS=OFF` disables native discovery.

Native discovery lives in `src/centypo/system`. Its CMake file selects one
implementation of the private `font_provider` API for Windows, Apple, Android,
or Fontconfig. The base provider returns empty results for unsupported operations;
disabled discovery instantiates it directly.

```sh
cmake -S . -B build -DCENTUI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run these test commands from the repository root. Centypo tests live in
`tests/unit` and use the shared font fixture under `tests/assets`.
