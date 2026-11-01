---
layout: default
title: Examples
nav_order: 8
---

# Examples worth reading

Each example is a complete app with a different lesson. Choose the one closest to what you are building.

| Example | Read it to learn | Build target |
| --- | --- | --- |
| [Getting started](https://github.com/swordfatih/centui/blob/main/tests/examples/getting_started/main.cpp) | A widget tree, named styles, and a callback. | `centui_example_getting_started` |
| [Task list](https://github.com/swordfatih/centui/blob/main/tests/examples/task_list/main.cpp) | A virtualized list with adding, removing, and completion state kept in application data. | `centui_example_task_list` |
| [Scene overlay](https://github.com/swordfatih/centui/blob/main/tests/examples/overlay/main.cpp) | Controls over an existing SFML scene and event loop. | `centui_example_overlay` |
| [Custom widget](https://github.com/swordfatih/centui/blob/main/tests/examples/custom_widget/main.cpp) | An animated meter with retained paths and a gradient. | `centui_example_custom_widget` |
| [Dashboard](https://github.com/swordfatih/centui/blob/main/tests/examples/dashboard/main.cpp) | Reusable styles, theme switching, scrolling, and a custom chart. | `centui_example_dashboard` |

## Build one

From a checkout, build the task list with SDL and bgfx:

```sh
cmake -S . -B build -DCENTUI_BUILD_EXAMPLES=ON -DCENTUI_BUILD_SDL=ON -DCENTUI_BUILD_BGFX=ON -DCENTUI_BUILD_TESTS=OFF -DCENTUI_BUILD_SFML=OFF -DCENTUI_BUILD_GLFW=OFF
cmake --build build --target centui_example_task_list
```

The executable is under `build/tests/examples/task_list/`. Multi-configuration generators add a directory such as `Debug`. The other examples use the same pattern; enable `CENTUI_BUILD_SFML` for the overlay.

All examples use system fonts. They do not need a copied font directory or a particular working directory.

Return to the guide for [dynamic interfaces](interaction.md), [scene integration](backends.md), or [custom widgets](extending.md) while reading the source.

[← Custom widgets](extending.md) · [Reference →](reference.md)
