# CentUI

A small retained UI library using C++26 modules and reflection, Yoga layout, and FreeType/HarfBuzz text. Widgets use typed handles and ordinary inheritance; SFML and bgfx implement the same renderer interface.

```cpp
#include <initializer_list>
import centui;
import centui.app;

int main()
{
    centui::app app{{.title = "CentUI", .width = 800, .height = 600}};
    auto title = centui::text{"Hello"};
    auto action = centui::button{"Update", {.on_click = [title]() mutable
    {
        title.set_text("Updated");
    }}};
    auto root = centui::container{title, action};
    centui::document document{root};
    app.run(document);
}
```

The styled example in `examples/main.cpp` is unchanged. The `container<Styles...>`, `text<Styles...>`, and `button<Styles...>` handles remain convenient constructors over ordinary widget classes. Style packs resolve into style values at compile time; runtime styles are also accepted. Unknown fields and incompatible property types are rejected. Runtime `patch_style` validates the complete candidate before changing the widget.

`examples/widgets.cpp` demonstrates toggle, range, single-line and multiline input, a selection list inside a scroll container, a composed popup, and caller-advanced easing. Its target is `centui_example_widgets` and requires `CENTUI_BUILD_APP`.

`examples/widgets_static.cpp` presents the same showcase with compile-time style structs and initializer-list composition, like `main.cpp`. The `toggle`, `range`, `selection`, `scroll`, `popup`, and `input` handles accept style packs directly. Constructor options provide initial values and callbacks; runtime operations remain available through `->`. Only widgets referenced elsewhere need named handles. Its target is `centui_example_widgets_static`.

## Ownership

Widget copies alias one stable object. Parents and handles share ownership; parent and document links are weak. Removing a subtree detaches it; surviving handles remain usable. Document destruction releases its root without invalidating external handles. Empty or moved-from handles reject access.

Each widget has one parent and each root belongs to at most one document. Duplicate children, cycles, and attaching already-owned children are errors. Widgets expose `append`, `remove`, `children`, and `parent` both detached and mounted. Mounted edits delegate to the document; document convenience operations use the same ownership checks. Structural changes made by callbacks are deferred until the callback finishes and complete before the next input event. Widgets, documents, and font services are confined to one application thread.

Use `weak()` when a callback captures its own widget or an ancestor, and check the handle returned by `lock()`. Strong captures in those cases would create ownership cycles. Capturing a sibling, as in the example above, is safe unless another callback creates a cycle back.

## Custom widgets

Derive from `centui::widget` and override the hooks you need:

```cpp
class swatch final : public centui::widget
{
protected:
    centui::size measure(const centui::measure_context&) override
    {
        return {64, 24};
    }

    void paint(centui::painter& output) const override
    {
        output.rectangle(output.bounds(), centui::color::from_hex(0x38BDF8));
    }
};

auto sample = centui::make_widget<swatch>();
auto root = centui::container{sample};
```

Setters check mutation permission before changing fields and call `invalidate_layout()` or `invalidate_paint()`. `measure()` is used for leaves; Yoga lays out widgets with children. Measurement receives exact/maximum/unconstrained axis constraints and a CPU text service. The framework paints backgrounds and traverses children; hooks paint content in document coordinates. Measurement and painting may not mutate the tree or recursively prepare it. See `examples/custom_primitive.cpp` for a complete mutable widget.

Widgets can inspect `bounds()`, `content_bounds()`, `hovered()`, `pressed()`, `focused()`, and `effectively_enabled()`. `enabled()` reports the widget's own setting; effective enablement includes ancestors. `appearance()` and the existing `style()` accessor return the same style value.

Derived containers can use `set_clip_children(true)` to clip descendants to their content rectangle and `set_child_offset({x, y})` to translate descendants without changing their Yoga allocation. Offsets accumulate through nested containers. Rendering, public bounds, and hit testing use the same translated geometry and clipping; captured gestures continue outside the clip. The container owns any scrolling policy or offset limits.

`set_focusable()` is independent of `set_interactive()`. Mounted, effectively enabled focusable widgets can call `request_focus()`, and receive `on_focus_changed(bool)`. A request made in a callback is deferred and revalidated when applied; its return value reports acceptance, not immediate focus. Disabling, detaching, or losing host focus clears focus and notifies the widget. Document destruction silently detaches surviving handles. A primary pointer press focuses a focusable target; other pointer buttons do not change focus.

`bool on_event(const input_event&)` returns whether an event was handled. Uncaptured pointer motion falls back through interactive ancestors until handled. Presses, captured moves, releases, and cancellations belong only to the gesture target; an unhandled gesture does not activate an ancestor. Keyboard/text events bubble from the focused widget, wheel events from the pointer target. Unhandled Tab traverses focus within the nearest focus scope.

Interactive widgets receive the press, captured moves, and matching release even outside their bounds or the viewport. The document tracks one widget/button pair; extra presses and unmatched releases do not steal or end that gesture. Primary, secondary, and middle buttons can initiate capture. Release ends capture before calling widget code. Built-in buttons recognize clicks themselves: a primary press followed by release over the same visible hit target. Leaving the widget is not cancellation; focus loss or invalidation of the captured target interrupts capture, including invalidation outside event dispatch. Hover and pressed styling remain managed by the document and refresh after geometry or targeting changes beneath a stationary pointer.

## Composing and extending buttons

`button_widget` owns activation behavior and accepts arbitrary child content. Override its protected `activate()` to implement a specialized control, and call the base implementation when its click callback should also run. `text_button_widget` adds text presentation, sharing text layout/cache code with `text_widget`. The existing `button<Styles...>` convenience handle constructs a text button.

```cpp
auto label = centui::make_widget<centui::text_widget>("Save");
auto action = centui::make_widget<centui::button_widget>(label);
auto root = centui::make_widget<centui::container_widget>();
root->append(action);
action->set_on_click([label] { label->set_text("Saved"); });
centui::document document{root};
```

Child content has its own style. Use runtime style values when constructing these widget classes; style-pack wrappers are optional.

## Backends

Every backend implements the same base:

```cpp
class renderer
{
public:
    virtual ~renderer() = default;
    virtual centui::viewport viewport() const noexcept = 0;
    virtual void draw(const centui::draw_list&) = 0;
};
```

`centui::ui ui{backend};` borrows a renderer and forwards rendering/input to a document. Backends upload bitmap resources and draw rectangles, images, and prepared glyph runs. Font selection, shaping, measurement, interaction, and tree traversal belong to the core. See `examples/custom_backend.cpp`.

Draw lists are owned by the document and borrowed until its next preparation or destruction. Commands retain CPU resources while live. Rendering is synchronous; backends must retain anything needed afterward. There is no retained-frame or raw-node API.

The host clears, presents, translates native input, and owns the graphics context. Destroy backend GPU resources before the context. The optional SDL/bgfx `app` handles this order. SFML borrows its render target and restores its view/graphics state.

`viewport{width, height, scale}` describes framebuffer pixels and the effective display scale. Its logical dimensions are derived by division. Layout, draw-command bounds, pointer events, wheel deltas, and text-input areas use logical units. Renderers map these units to the framebuffer; text is rasterized at `font_size * scale`, with glyph origins aligned to physical pixels. Display-scale changes refresh layout and prepared text. Widgets do not manage DPI.

SDL supplies scale automatically and converts native input and IME coordinates. The SFML backend accepts the host's scale in its constructor and through `set_scale()`; its example queries Windows DPI, while other platforms currently use scale 1 and require host integration for automatic DPI detection. Custom hosts must distinguish native window coordinates from CentUI logical coordinates. Text-layout caches must account for `text_service::scale()` as well as font identity and size.

## Text and rendering scope

System fonts load lazily. Explicit fonts, UTF-8 byte clusters, LF/CRLF, ASCII-space wrapping, line alignment, and content/viewport text clipping are supported. Long words overflow intact and clip. CPU and GPU caches have bounds on cached ownership; live draw resources can retain additional memory.

Coordinates start at the top left with Y downward. Colors use straight RGBA. Styles provide visibility, overflow clipping, and rectangular borders. Scrolling behavior belongs to `scroll_widget`; overflow styling alone does not implement wheel handling or offset limits.

`centui.controls` provides toggle, range, scroll, selection, and popup widgets. `centui.input` provides a single-line or multiline editor with selection, clipboard hooks, composition preview, and undo/redo. Construct these through `make_widget<T>()` and compose ordinary children. The optional `centui.animation` module provides caller-advanced numeric animations; `centui.easing` can be used independently.

The SDL host translates keyboard, text, composition, and wheel input and supplies clipboard/text-input services. The SFML example still translates pointer input only. Full Unicode paragraph bidi, grapheme editing, fallback-font chains, and accessibility remain outside this milestone. See [implementation status](docs/implementation-status.md) for validation and remaining work.

## Build boundaries and verification

The library uses ordinary named modules, without partitions:

| Module | Responsibility |
| --- | --- |
| `centui.types` | Geometry, colors, input, units, font identifiers |
| `centui.style` | Reflected style composition, validation, patches |
| `centui.render` | Portable glyph/drawing data and renderer contract; depends only on types |
| `centui.font` | Font resources, shaping, glyph caches; produces rendering data |
| `centui.layout` | Yoga integration |
| `centui.tree` | Widget, handles, document, painter, and UI facade together |
| `centui.widgets` | Container, text, button, and image |
| `centui.controls` | Reusable control behavior and composition |
| `centui.input` | Text editing behavior |
| `centui.easing`, `centui.animation` | Optional easing functions and caller-advanced values |
| `centui` | Public umbrella |

SFML, bgfx, window, and app remain separate modules. Widget/document friendship is local to `tree.cppm`. There is no separate `document.cpp` or cross-module accessor bridge.

The `centui::centui` target is headless. Optional targets are `centui::sfml`, `centui::bgfx`, and `centui::app`. CMake options `CENTUI_BUILD_SFML`, `CENTUI_BUILD_BGFX`, `CENTUI_BUILD_APP`, `CENTUI_BUILD_TESTS`, and `CENTUI_BUILD_EXAMPLES` control them. Consumers needing the app must link `centui::app`; the core does not acquire graphics dependencies.

Requires CMake 4.3+, Ninja, and a compiler/standard library supporting C++26 modules, `<meta>`, reflection expressions, and expansion statements with `-freflection`.

The widget extension API refactor has been reviewed as source only. At the user's instruction, no configuration, compilation, tests, or application runs were performed for this refactor. Test sources describe intended behavior and are not evidence of a passing build.
