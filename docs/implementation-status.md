# Widget foundation milestone

Source checkpoint, 2026-10-22. No configuration, compilation, tests, or application runs were performed for this milestone. Existing test sources have not yet been extended to cover the new behavior. This is not a verified release.

## Implemented in source

- Core mechanisms: keyboard/text/composition and wheel routing, Tab traversal and focus scopes, host clipboard/text-input hooks, layout notification, shared clipping and translated geometry.
- Styles: visible/hidden/collapsed, overflow, rectangular borders.
- Widget behavior: keyboard buttons, toggle, range, scrolling, selection, popup, and a single editor supporting single-line/multiline text, selection, composition preview, clipboard, and bounded undo/redo.
- Rendering: CPU bitmap/image commands, image widget, and SFML/bgfx image uploads; shaping-derived caret and selection geometry.
- Optional easing and numeric animation modules, advanced explicitly by the caller.
- SDL translation and host services. Existing `examples/main.cpp` is unchanged.
- Shared platform interface, SDL platform independent of bgfx, and SFML owned/borrowed window adapters. `ui` forwards native events and attaches host services; `run` provides the optional loop. bgfx owns its graphics context when constructed from a platform. `app` is a thin composition of these pieces.
- DPI: viewport stores framebuffer size and effective UI scale; layout/input remain logical, glyph rasterization uses physical resolution, and both renderers map to framebuffer pixels. SDL detects display scale automatically; SFML accepts host-provided scale (Windows detection in the example). DPI regression sources were added but not executed.

Containers remain ordinary widgets. Clipping is styling; scrolling adds offset limits and input behavior in `scroll_widget`. Selection, popup policy, editing, and animation do not become document-wide control systems. New controls use `make_widget<T>()`; existing container/text/button convenience APIs remain.

## Next milestone

1. Review and add focused regression coverage for routing/focus, nested scrolling, composition cache invalidation, undo, popup focus restoration, image clipping, and animation endpoints. Compile and execute only when authorized.
2. Validate the new platform adapters on actual windows. SDL and SFML now provide keyboard/text/wheel translation and clipboard services; SFML supports owned and borrowed windows. SFML composition previews and IME placement are unavailable through its public API. SDL/bgfx native window bridging and automatic SFML DPI detection currently target Windows.
3. Refine editor caret behavior for graphemes, ligatures, and bidirectional text. Current movement uses shaping clusters; this is not a complete Unicode editing implementation. Review native IME placement and lifecycle on actual hosts.
4. Review popup placement and modality requirements. The current full-parent popup supports dismissal, focus restoration, and Tab scope; it has no automatic anchoring or strict programmatic focus confinement.

Scrollbars, rounded drawing, additional visual presets, and composite examples are deferred. The component catalog is not implemented as separate widget classes; many entries should be compositions over these foundations.
