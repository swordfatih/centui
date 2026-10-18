# CentUI: core, backends, and optional application runner

## Standalone application

```cpp
import centui.app;
import centui.container;
import centui.text;

int main()
{
    centui::app app{{.title = "CentUI", .width = 800, .height = 600}};
    centui::container<panel_style> root{centui::text<>{"Hello"}};
    app.run(root);
}
```

For application work between frames:

```cpp
while(app.poll_events())
{
    // Update application state and elements.
    app.render_frame(root);
}
```

`app` is optional. Its SDL3/bgfx implementation owns the window, graphics context,
backend and UI. It handles pixel-size changes, swap-chain reset and presentation.
`render_frame` presents exactly once after a successful drawable frame. `run`
adds the event loop. Neither function retains the element after returning.
The current native-window implementation remains Windows/D3D11-specific.

## Embedded UI / HUD

```cpp
import centui.bgfx;
import centui.ui;

// Host has already initialized bgfx and reserved view 10 for this HUD.
centui::viewport viewport{800, 600};
centui::bgfx::backend backend{viewport, 10};
centui::ui hud{backend};

// Inside the host frame:
viewport = {drawable_width, drawable_height};
hud.render(root);
// The host presents the frame.
```

The host owns its window, input, graphics context, reset policy, scene rendering,
view ordering and presentation. The backend owns only UI graphics resources and
its font. It reads the borrowed viewport and draws in its reserved view without
clearing, changing framebuffer binding or presenting. It never calls bgfx init,
shutdown, reset or frame. The host should use an exclusive view, arrange its
ordering after scene views and disable its clear for overlays.

Lifetime order: viewport/context outlive backend; backend outlives UI. Destroy the
backend before bgfx shutdown. Temporary viewport binding is rejected. The backend
is immovable; UI borrows it by reference. Do not replace its font while its glyph
cache is in use. The app enforces the correct order through member destruction:
UI, backend handles/font, then graphics context and SDL window.

## Concrete backend contract

`ui<backend_type>` has no SDL or bgfx import. It calls:

- `backend.viewport()` for pixel dimensions;
- `backend.glyphs()` for a mutable `centui::font&` used to prepare text;
- `backend.draw(span<const draw_command>)` to synchronously consume commands.

No virtual interface or type erasure is involved. Zero-sized viewports skip
preparation and submission. The SFML backend borrows an SFML render target,
uploads shared glyph bitmaps to SFML textures, and consumes the same commands
without calling `display()`.

## Modules and build targets

| Target | Modules / responsibility | Dependencies |
| --- | --- | --- |
| `centui::values` | unit, color, viewport | Standard library |
| `centui::glyph` | glyph; font compatibility re-export | Values, FreeType, HarfBuzz |
| `centui::core` | style, layout, node, draw, element, container, text, ui | Glyph, Yoga |
| `centui::bgfx` | bgfx backend, GPU resources, shaders | Core, bgfx |
| `centui::app` | app and optional window implementation | bgfx backend, SDL3 |
| `centui::sfml` | renderer over a borrowed SFML render target | Core, SFML Graphics |

`centui.glyph` contains shared font loading, shaping, measurement and rasterization;
the concrete font type remains `centui::font`. `import centui.font` remains a
compatibility re-export. `centui.renderer` is replaced by `centui.bgfx` and
`centui::bgfx::backend`. The prior UI viewport constructor is replaced by `ui{backend}`.
Consumers link the narrow target they use. SFML is fetched only when
`CENTUI_BUILD_SFML` is enabled; SDL and bgfx stay in their own optional targets.

The bgfx implementation, dependency declaration, shader compiler helper and shader
sources live under `src/backends/bgfx/`. Generated shader headers live in that
backend's build directory and are private includes. SDL dependency setup lives
under `src/app/` and SFML setup lives under `src/backends/sfml/`.
Core dependency setup never fetches SDL or bgfx. Module lists are explicit.

The default build includes the bgfx app, SFML backend and examples. To build only
the headless core/tests:

```sh
cmake -S . -B build-headless -G Ninja -DCENTUI_BUILD_BGFX=OFF -DCENTUI_BUILD_APP=OFF -DCENTUI_BUILD_SFML=OFF -DCENTUI_BUILD_EXAMPLES=OFF
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
```

Use the existing GCC 16.2/C++26 modules toolchain. For embedded bgfx, leave
`CENTUI_BUILD_BGFX=ON` and set `CENTUI_BUILD_APP=OFF`. The app option requires bgfx;
inconsistent configuration fails explicitly. Headless tests link only core.

## Tree, commands and text

Elements privately own movable, noncopyable trees. Applications use `style()` and
`text::value()` rather than `.node`. The internal node API remains available for
low-level tests and construction through `details::element_access`.

Preparation currently synchronizes/recalculates the tree and reshapes text on each
render. This deliberately avoids stale caches after mutation, moves or replacement.
Selective invalidation is future performance work. Text nodes must be leaves.
Updates are not transactional; recover invalid input before rendering again.

Text-layout results contain glyph positions, dimensions and ascender. Yoga callbacks
capture dimensions, not fonts or element addresses. Rendering uses that same result.
Commands borrow the prepared tree and must be consumed before its next mutation,
move, preparation or destruction. Backends must not retain command spans.

Painter order is parent background, parent text, then children. The bgfx backend
uses sequential view mode, preserving order across shader changes. Clipping,
wrapping, focus/input, scrolling and CSS inheritance remain unimplemented.

## Limitations and verification

The app and bgfx backend compile, including regenerated shaders. The headless suite
includes a recording backend that exercises the actual templated UI coordination,
resize behavior and moved roots without initializing graphics. A separate build
with bgfx/app disabled verifies that SDL, bgfx and shader tools are absent from its
build graph. Actual graphics output and interactive SDL resizing still require a
GPU smoke test; this change does not claim runtime GPU verification.

Existing limitations remain: default font discovery is Windows-only, glyph textures
are cached until backend destruction, extreme glyph dimensions need texture-limit
handling, and the window helper assumes a single bgfx session. `position::fixed`
retains its historical Yoga-static mapping; text alignment/padding semantics need
future work. No toolchain change or compiler/linker suppression was introduced.

## Ownership conventions

Public optional borrows use C++26 `std::optional<T&>`: text commands hold
`std::optional<const text_layout&>` and low-level node operations accept
`std::optional<font&>`. Use `attach(face)` or `attach()`; the old pointer overload
is removed. Optional references do not extend lifetimes, so command borrowing
rules are unchanged. Empty layout references preserve the backend shaping fallback.

Native FreeType/HarfBuzz handles use `unique_ptr` with custom deleters;
`std::out_ptr` bridges C output parameters. Measurement callbacks use
`std::move_only_function` with stable heap storage for Yoga's context pointer.
Window titles use owning `std::string` in options and a string reference in the
window constructor; `.c_str()` appears only at SDL's API call. GPU transient data
is viewed through `std::span`, and projection storage uses `std::array`.
Raw handles, pointer casts, C strings and null sentinels remain where required by
external APIs. Local `std::get_if` pointer results remain idiomatic variant access;
there is no raw owning pointer in CentUI's font resource state.

## Source layout and SFML backend

Production code lives under `src/core`, `src/backends`, and `src/app`. The old
starter-only `include/` and `src/start.cpp` have been removed. Examples and tests
remain at repository root. `src/backends/sfml` fetches SFML 3.1.0 and implements a
renderer over a borrowed `sf::RenderTarget`. It shares CentUI glyph shaping,
rasterization, layout, and painter order. The example owns event polling, clear,
and display. The SFML renderer only submits textured quads and rectangles.
Enable/disable it with `CENTUI_BUILD_SFML`; the bgfx app and backend remain
independently controlled by their existing options.
