---
layout: default
title: Performance
parent: Reference
nav_order: 2
---

# Performance

Start with a clear widget tree and update only the values that changed. CentUI retains layout state and caches text and drawing resources; you do not need to rebuild the document for each frame.

## Measure a real workload

Watch `document.statistics()` while scrolling, editing text, resizing, or updating your app's data. It reports layout work, painting, and command-cache use. Backend `cache_statistics()` reports texture-cache activity. Compare these counters with actual frame time before changing cache settings.

Repeatedly assigning an unchanged built-in value should not require a manual invalidation. A custom widget that reads external state must invalidate when that state changes; the framework cannot discover that dependency.

## Reuse a quiet panel

When a static panel sits beside frequently changing content, try a paint boundary:

```cpp
panel->set_paint_boundary(true);
```

This can reuse prepared drawing commands while that subtree's appearance and geometry remain unchanged. Resizing, scrolling, inherited style changes, and resource changes can invalidate the reuse. It does not eliminate submitting the visible drawing commands to the renderer.

Measure again before applying boundaries to more widgets.

## Set budgets when memory matters

| Resource | Control |
| --- | --- |
| Prepared drawing commands | `document.set_command_cache_budget(bytes)` |
| Retained paint rasters | `document.set_paint_cache_budget(bytes)` |
| Text analysis, layout, and glyph images | `document.set_text_cache_budgets(budgets)` |
| Renderer textures | `renderer.set_cache_budget(bytes)` |

Retained layouts and active drawing can keep resources alive beyond cache eviction. Cache budgets are not a cap on total process or GPU memory. Tune them against a representative workload, and keep the defaults until measurement gives you a reason to change them.

[← Reference](reference.md) · [Custom widget invalidation](extending.md#tell-the-document-what-changed)
