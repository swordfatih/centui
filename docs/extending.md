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

Call `check_mutation()` before changing widget state. Use `invalidate_paint()` when only the appearance changes, or `invalidate_layout()` when the natural size or child arrangement changes. Measurement and painting should only read the tree. Widgets with children are laid out by Yoga rather than by their `measure()` hook. The complete [custom primitive](https://github.com/swordfatih/centui/blob/main/examples/custom_primitive/main.cpp) is a small starting point.

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

Call `set_interactive()` if the widget should receive pointer events, and `set_focusable()` if it can take keyboard focus. Override `on_event(const centui::input_event&)` and return `true` for events you handle. `on_focus_changed(bool)` lets you react to focus transitions. Read `hovered()`, `pressed()`, `focused()`, and `effectively_enabled()` when painting an interaction state.

`pressable_widget` already implements ordinary button activation. Derive from it and override `activate()` for a specialized action; call the base method if you also want the registered `on_click` callback. Build composite controls from children when their content should stay reusable.

Containers can call `set_clip_children(true)` or `set_child_offset({x, y})` when they need clipping or scrolling. The [scroll widget](https://github.com/swordfatih/centui/blob/main/src/core/widgets/scroll.cpp) shows how to clamp an offset and handle wheel and page keys.
