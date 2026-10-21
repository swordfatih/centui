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

The styled example in `examples/main.cpp` retains its construction and behavior with updated imports. Style packs are resolved at compile time; unknown fields and incompatible property types are rejected. Runtime `patch_style` validates the complete candidate before changing the widget.

## Ownership

Widget copies alias one stable object. Parents and handles share ownership; parent and document links are weak. Removing a subtree detaches it; surviving handles remain usable. Document destruction releases its root without invalidating external handles. Empty or moved-from handles reject access.

Each widget has one parent and each root belongs to at most one document. Duplicate children, cycles, and attaching already-owned children are errors. Use document operations for mounted topology. Structural changes made by input callbacks complete before the next event. Widgets, documents, and font services are confined to one application thread.

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

Setters check mutation permission before changing fields and call `invalidate_layout()` or `invalidate_paint()`. Measurement receives exact/maximum/unconstrained axis constraints and a CPU text service. The framework paints backgrounds and traverses children; hooks paint content in document coordinates. Measurement and painting may not mutate the tree or recursively prepare it. See `examples/custom_primitive.cpp` for a complete mutable widget.

Interactive widgets receive the press, captured moves, and matching release even outside their bounds or the viewport. The document tracks one widget/button pair; extra presses and unmatched releases do not steal or end that gesture. Primary, secondary, and middle buttons can initiate capture. Release ends capture before calling widget code. Built-in buttons recognize clicks themselves: a primary press followed by release inside the button. Leaving the widget is not cancellation; focus loss or invalidation of the captured target interrupts capture. Hover and pressed styling remain managed by the document.

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

`centui::ui ui{backend};` borrows a renderer and forwards rendering/input to a document. Backends only upload glyph images and draw rectangles/prepared glyph runs. Font selection, shaping, measurement, interaction, and tree traversal belong to the core. See `examples/custom_backend.cpp`.

Draw lists are owned by the document and borrowed until its next preparation or destruction. Commands retain CPU resources while live. Rendering is synchronous; backends must retain anything needed afterward. There is no retained-frame or raw-node API.

The host clears, presents, translates native input, and owns the graphics context. Destroy backend GPU resources before the context. The optional SDL/bgfx `app` handles this order. SFML borrows its render target and restores its view/graphics state.

## Text and rendering scope

System fonts load lazily. Explicit fonts, UTF-8 byte clusters, LF/CRLF, ASCII-space wrapping, line alignment, and content/viewport text clipping are supported. Long words overflow intact and clip. CPU and GPU caches have bounds on cached ownership; live draw resources can retain additional memory.

Coordinates start at the top left with Y downward. Colors use straight RGBA. Widget overflow remains visible outside its parent, except for text's own content clipping and the viewport. Unicode paragraph bidi, fallback-font chains, editable text, keyboard navigation, accessibility, scrolling, and animations are outside this implementation.

## Build boundaries and verification

The core uses eight ordinary named modules, without partitions:

| Module | Responsibility |
| --- | --- |
| `centui.types` | Geometry, colors, input, units, font identifiers |
| `centui.style` | Reflected style composition, validation, patches |
| `centui.render` | Portable glyph/drawing data and renderer contract; depends only on types |
| `centui.font` | Font resources, shaping, glyph caches; produces rendering data |
| `centui.layout` | Yoga integration |
| `centui.tree` | Widget, handles, document, painter, and UI facade together |
| `centui.widgets` | Container, text, and button |
| `centui` | Public umbrella |

SFML, bgfx, window, and app remain separate modules. Widget/document friendship is local to `tree.cppm`. There is no separate `document.cpp` or cross-module accessor bridge.

The `centui::centui` target is headless. Optional targets are `centui::sfml`, `centui::bgfx`, and `centui::app`. CMake options `CENTUI_BUILD_SFML`, `CENTUI_BUILD_BGFX`, `CENTUI_BUILD_APP`, `CENTUI_BUILD_TESTS`, and `CENTUI_BUILD_EXAMPLES` control them. Consumers needing the app must link `centui::app`; the core does not acquire graphics dependencies.

Requires CMake 4.3+, Ninja, and a compiler/standard library supporting C++26 modules, `<meta>`, reflection expressions, and expansion statements with `-freflection`.

This rewrite has only been reviewed as source. At the user's instruction, no configuration, compilation, tests, or application runs were performed. Test sources describe intended behavior and are not evidence of a passing build. See `docs/rewrite-status.md` for the verification limits and `IMPLEMENTATION_PLAN.md` for the architecture and acceptance criteria.
