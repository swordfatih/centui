---
layout: default
title: Extending CentUI
nav_order: 4
---

# Extending CentUI

You can make a widget by deriving from `centui::widget`. A leaf reports its natural size with `measure()` and draws inside its content bounds with `paint()`:

```cpp
class meter final : public centui::widget
{
public:
    explicit meter(float value = 0) : m_value(value)
    {
    }

    void set_value(float value)
    {
        check_mutation();
        m_value = value;
        invalidate_paint();
    }

protected:
    centui::size measure(const centui::measure_context&) override
    {
        return {120, 24};
    }

    void paint(centui::painter& output) const override
    {
        auto bar = output.bounds();
        bar.width *= m_value;
        output.rectangle(bar, centui::color::from_hex(0x38BDF8));
    }

private:
    float m_value;
};

auto progress = centui::make_widget<meter>(0.25f);
```

Call `check_mutation()` before changing widget state. Framework setters propagate their own changes; callers do not repeat invalidation. Custom setters choose the work they require:

| Request | Contract |
| --- | --- |
| `invalidate_layout()` | Content metrics, measurement/baseline inputs, or intrinsic requirements changed. Synchronize layout, calculate bounds, refresh geometry, and repaint. This is the conservative option. |
| `invalidate_geometry()` | Rebuild geometry derived from current allocated bounds, such as paths, caret placement, or reveal calculations. Includes paint invalidation without requesting Yoga calculation. |
| `invalidate_paint()` | Drawing changed with no geometry preparation required. |

`measure()`, baseline evaluation (including `baseline()`), and `paint()` must not mutate widgets or request more work. They may retain internal derived caches. Widgets with children are laid out by Yoga rather than by their `measure()` hook. The complete [custom primitive](https://github.com/swordfatih/centui/blob/main/examples/custom_primitive/main.cpp) is a small starting point.

`on_geometry()` is the sole geometry preparation hook. It runs after current layout and placement, before painting and host text-input synchronization. It runs on mounting, after layout/placement changes, and after explicit geometry invalidation even when the widget rectangle is unchanged. Explicit requests coalesce and do not retain removed widgets. Requests made while detached or without a valid viewport survive until geometry becomes available. A failed hook and unfinished work remain pending for retry.

Geometry hooks may change child offsets or request intrinsic layout, but must converge. Changed offsets cause another placement pass before dependent geometry and targeting settle. Do not unconditionally reschedule the hook, and do not invoke lifecycle hooks directly from event handlers; share a private helper when immediate hit-test geometry is needed.

Command caching does not discover dependencies on external state or another widget. A custom widget reading those values must explicitly invalidate when they change. Requesting an animation frame alone does not imply changed layout, geometry, or drawing; invalidate the state changed by the frame callback.

## Baselines

Override `baseline()` when a custom widget should align something other than its
bottom edge with neighboring text:

```cpp
centui::baseline_function baseline() override
{
    return [](centui::size content)
    {
        return content.height * 0.75f;
    };
}
```

The callback receives the content size and returns a finite offset from its top,
in logical units. CentUI adds the resolved top padding and border. Like
measurement, it runs during layout and must not mutate the widget tree. Callback
exceptions are captured at the Yoga boundary and rethrown during preparation.
Returning an empty function uses Yoga's normal baseline selection from children
or, for a leaf, the bottom edge. `reference_baseline` selects which descendant a
container uses. Text and input widgets provide a first-line font baseline;
scrolling input content does not change the input's layout baseline.

## Input and focus

Custom widget constructors should accept `centui::style_source appearance = {}` and forward it to the base `widget` constructor. `make_widget<meter>(..., centui::style_source::composed<meter_style>())` retains named styles so document patches can reach them. A full `centui::style` converts to a complete local replacement. Style recipes reconstruct their definitions during updates, so keep style constructors deterministic and side-effect-free.

Call `set_interactive()` if the widget should receive pointer events, and `set_focusable()` if it can take keyboard focus. Override `on_event(const centui::input_event&)` and return `true` for events you handle. `on_focus_changed(bool)` lets you react to focus transitions. Read `hovered()`, `pressed()`, `focused()`, and `effectively_enabled()` when painting an interaction state.

`pressable_widget` already implements ordinary button activation. Derive from it and override `activate()` for a specialized action; call the base method if you also want the registered `on_click` callback. Build composite controls from children when their content should stay reusable.

Containers can call `set_clip_children(true)` or `set_child_offset({x, y})` when they need clipping or scrolling. The [scroll widget](https://github.com/swordfatih/centui/blob/main/src/centui/widgets/scroll.cpp) shows how to clamp an offset and handle wheel and page keys.

## Vector painting

Build reusable geometry with `centui::path`: `move_to`, `line_to`, `curve_to`,
`close`, `rectangle`, `rounded_rectangle`, and `circle`. Copies share storage
until one is edited. Keep paths as widget members and rebuild size-dependent
geometry in `on_geometry()`, using `content_bounds()` for the current dimensions.
Geometry hooks run after bounds are stored and may request a frame. If a hook
throws, preparation retries the failed and unfinished requests on its next call.

```cpp
void paint(centui::painter& output) const override
{
    output.fill(
        m_shape,
        centui::linear_gradient{
            {0, 0},
            {120, 0},
            {
                {0, centui::color::from_hex(0x38BDF8)},
                {1, centui::color::from_hex(0x5EEAD4)}
            }
    });
    output.stroke(m_shape, centui::color::from_hex(0xFFFFFF), 2);
}
```

All painter operations use local content coordinates, including paths, text,
rectangles, and images. `painter::bounds()` starts at `(0, 0)` and reports the
content size. Widget `bounds()` and `content_bounds()` remain in document
coordinates for layout and input; do not pass them directly to the painter.
Brushes accept a solid color, a linear gradient, or a radial gradient. Gradient
coordinates use the same local space as the path; stops must be nonempty and
ordered from zero to one. Stroke widths and dimensions must be finite and
nonnegative; path coordinates may be negative. Strokes use round caps and joins.

Paths and rounded boxes rasterize at the viewport scale into ordinary textured
quads. Only tiles intersecting the inherited clip and viewport are prepared.
Each bitmap is at most 512 by 512 physical pixels, including filtering borders;
the tile grid stays anchored to the shape so scrolling can reuse cached tiles.
Gradients retain their original coordinates across tiles. Transformed vector
bounds, including stroke and antialias padding, must stay within +/-4,194,304
physical pixels; larger coordinates throw `std::length_error` before rasterization.
This limit protects the rasterizer's coordinate representation, independently
of the much smaller per-texture limit.

Each document retains a raster cache with a default budget of 16 MiB. Use
`document.set_paint_cache_budget(bytes)` to change it, or zero to disable cache
retention. This budget covers retained cache entries, not all live memory:
prepared draw lists also own bitmap references, and rasterization uses temporary
storage. Previously prepared lists remain valid after cache eviction or document
destruction. Copy a draw list if it must outlive the next call to `prepare()`.
Tiling bounds individual raster surfaces; work still depends on visible tiles,
path complexity, and overlapping widgets.

Styles support `corner_radius` for backgrounds and borders. It affects painting
without changing layout; descendants retain rectangular clipping bounds.

## Animation

Call `request_frame()` when a widget needs an update, then override
`on_frame(centui::frame_duration elapsed)`. Requests coalesce into one callback
per tick. Request another frame inside the callback to continue animating, and
call `invalidate_paint()` or `invalidate_layout()` when the displayed state
changes. Scheduling holds a weak reference, so it does not keep removed widgets
alive. A request made while detached is delivered after mounting.

`ui.frame(document)` processes polled events, calculates elapsed time, advances
widgets, and renders. Use `ui.frame(document, elapsed)` to supply your own timing.
When managing these steps separately, call `document.advance(elapsed)` once per
iteration before rendering; elapsed is a finite, nonnegative duration in seconds.
Tree edits made during animation
callbacks are committed after the tick batch, as they are during input dispatch.

The [custom primitive example](https://github.com/swordfatih/centui/blob/main/examples/custom_primitive/main.cpp)
demonstrates retained paths, a gradient, and animation driven by a slider. It
requires SDL and bgfx. The Pulse dashboard uses vector chart lines and a
gradient area fill.
