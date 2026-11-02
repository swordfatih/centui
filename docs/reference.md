---
layout: default
title: Reference
nav_order: 9
has_children: true
---

# Reference

Use this section when you have a specific question. The [guides](index.md) introduce the library in order.

| Need | Go to |
| --- | --- |
| CMake options, adapters, or system dependencies | [Build and integration](build.md) |
| A slow frame or growing caches | [Performance](performance.md) |
| A complete app to adapt | [Examples](examples.md) |
| Font layout outside the UI | [Centypo](https://github.com/swordfatih/centui/blob/main/src/centypo/README.md) |

## Public API

The headers contain the exact types, defaults, and contracts. Start at the area you need:

| Area | Header |
| --- | --- |
| Tree, themes, fonts, and focus | [document.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/document.hpp) |
| Layout and appearance | [style.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/style.hpp), [layout.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/layout.hpp) |
| Built-in controls | [widgets](https://github.com/swordfatih/centui/tree/main/include/centui/widgets) |
| Handles and ownership | [handle.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/handle.hpp) |
| Event loops and frame control | [ui.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/ui.hpp) |
| Input, platform events, and host services | [types.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/types.hpp) |
| Custom drawing | [widget.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/widget.hpp), [painter.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/painter.hpp) |
| Host and graphics integration | [platform.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/platform.hpp), [render.hpp](https://github.com/swordfatih/centui/blob/main/include/centui/render.hpp) |
