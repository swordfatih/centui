---
layout: default
title: "6. Custom widgets"
nav_order: 7
---

# Custom widgets

Most interfaces come from composing existing widgets. Derive from `centui::widget` when you need drawing or interaction specific to your app: a meter, timeline, chart, or viewport.

## Draw a meter

A leaf widget reports its preferred size and paints in local coordinates:

```cpp
class meter final : public centui::widget
{
public:
    explicit meter(centui::style_source appearance = {})
        : widget(appearance)
    {
    }

    void set_value(float value)
    {
        check_mutation();
        m_value = std::clamp(value, 0.0f, 1.0f);
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
    float m_value = 0;
};

auto progress = centui::make_widget<meter>();
progress->set_value(0.25f);
```

Include `<algorithm>` and `<centui/centui.hpp>` for this snippet. The containing layout decides the final size; `measure()` supplies the widget's natural content size. Styles still control padding, dimensions, and the rest of its appearance.

## Tell the document what changed

Call `check_mutation()` before changing custom widget state. Then request the work that state needs:

| Change | Request |
| --- | --- |
| Text, preferred size, or measurement inputs | `invalidate_layout()` |
| Cached paths or geometry derived from current bounds | `invalidate_geometry()` |
| Colors or other drawing-only state | `invalidate_paint()` |

Built-in setters handle their own invalidation. Keep `measure()` and `paint()` read-only: they must not edit widgets or request another frame. Prepare paths that depend on allocated size in `on_geometry()`.

## Animate only while needed

Call `request_frame()` to receive `on_frame(frame_duration elapsed)`. Update the animated state there, invalidate what changed, and request another frame while the animation is still moving.

The [custom-widget example](https://github.com/swordfatih/centui/blob/main/tests/examples/custom_widget/main.cpp) adds a target value, smoothing, a gradient, and rounded paths to this meter. It separates geometry preparation from per-frame changes.

## Draw text and richer shapes

`painter` provides rectangles, paths, gradients, images, clipping, and prepared text. Use `measure_context::typography` to lay out text, then rasterize through `painter::typography()` at `painter::scale()` for drawing. Keep a `centypo::layout` value when the widget needs to reuse it.

Read the [dashboard chart](https://github.com/swordfatih/centui/blob/main/tests/examples/dashboard/main.cpp) for a larger custom drawing example. The public [widget](https://github.com/swordfatih/centui/blob/main/include/centui/widget.hpp) and [painter](https://github.com/swordfatih/centui/blob/main/include/centui/painter.hpp) headers describe the remaining hooks when you need them.

[← An existing application](backends.md) · [Next: explore the examples →](examples.md)
