---
layout: default
title: Widgets and Styles
nav_order: 3
---

# Widgets and Styles

CentUI widgets are ordinary objects connected by parent and child relationships. The convenient `text`, `button`, `container`, `input`, and other handles make a tree readable at the call site:

```cpp
auto status = centui::text{"Waiting"};
auto root = centui::container{
    status,
    centui::button{"Go", {.on_click = [status]() mutable
    {
        status.set_text("Done");
    }}},
};
```

`button` is a text control. `pressable` handles the same focus, hover, pointer, and keyboard behavior around any content:

```cpp
auto tile = centui::pressable<tile_style>{
    centui::container{
        centui::text{"Project"},
        centui::text{"Open details"},
    },
    {.on_click = open_project},
};
```

Other controls are `toggle`, `range`, `selection`, `scroll`, `popup`, and single-line or multiline `input`. Their option structs carry initial values and callbacks. See [gui_static](https://github.com/swordfatih/centui/blob/main/examples/gui_static/main.cpp) for all of them together. You can also build the same tree with `make_widget<T>()` and `append()`; [gui_dynamic](https://github.com/swordfatih/centui/blob/main/examples/gui_dynamic/main.cpp) uses that approach.

## Give widgets a style

A style struct names only the properties you want to change:

```cpp
struct blue_button
{
    centui::length height = centui::px(44);
    centui::edges padding = centui::edges::all(centui::px(12));
    centui::states_color background_color =
        centui::states_color::all(centui::color::from_hex(0x2563EB));
};

auto save = centui::button<blue_button>{"Save"};
```

You can combine style structs, use a `centui::style` value at runtime, or change a mounted widget with `patch_style({.height = centui::px(52)})`. The layout system handles width, height, padding, margins, gaps, alignment, and direction. State colors can vary for normal, hover, press, focus, and disabled states. Text inherits a font from its ancestors or the document's font service.

## Document themes

Widgets retain their ordered style types and cache the resolved appearance. Change defaults or patch a named style across a document without recreating its widgets:

```cpp
struct base
{
    float font_size = 16;
};

struct secondary
{
    float font_size = 14;
};

auto save = centui::button<base, secondary>{"Save"};
centui::document document{centui::container{save}};

document.set_style({.text_color = centui::states_color::all({30, 40, 50, 255})});
document.patch_style<base>({.font_size = 20}); // Secondary still wins: 14.
save.patch_style({.font_size = 18});          // Final local override: 18.
save.clear_style_overrides();               // Back to 14.
document.clear_style_patch<base>();         // Restore the original Base definition.
```

Resolution applies built-in defaults, document defaults, then each composed type immediately followed by its document patch, and finally widget-local overrides. A patch matches an exact type, including when that type is used by different widget kinds or compositions. C++ base classes are not separately matched.

Document `set_style()` replaces the default override layer; `{}` clears it. Document defaults apply to every node, including controls' internal children, so box properties such as padding affect those children too. Document defaults are fallback values rather than inherited theme tokens.

Successive patches merge supplied fields into one current override; no patch history is retained. Widget patches remain explicit even when their values equal current defaults. `widget.set_style(style)` and explicit full-style constructor arguments make every property locally explicit; clearing local overrides restores the retained composition. Runtime nested patches replace entire supplied properties, as described below.

Changes resolve and validate affected widgets before committing the document settings and cached styles. Unchanged results do not invalidate anything; changes request layout or paint as appropriate. Rendering reads the cache. Patches also apply to widgets mounted later. Removal resolves without document layers; mounting in another document uses that document's settings. Style constructors used by retained recipes must be deterministic and side-effect-free.

The [Pulse example](../examples/pulse/main.cpp) switches between dark and light appearances using document patches. For custom widgets, accept a `centui::style_source` construction argument and pass it to `widget`; use `style_source::composed<Styles...>()` with `make_widget()` to retain named styles.

## Callback events

Callbacks can take an optional leading `centui::event&`; callback-specific values follow it:

```cpp
auto toggle = centui::toggle{
    "Dark mode",
    {.on_change = [](centui::event& event, bool dark)
    {
        event.document().set_style({
            .text_color = centui::states_color::all(
                dark ? centui::color{240, 240, 240, 255} : centui::color{30, 30, 30, 255}
            )
        });

        auto parent = event.target()->parent();
        if(parent.valid())
        {
            // Common widget operations are available through this handle.
        }

        if(auto input = event.input())
        {
            const auto& source = input->get();
            // Inspect source.type, source.mods, or source.code.
        }
    }}
};
```

`target()` is the widget emitting the semantic callback, such as the button rather than its internal label. It returns a `handle<>` with parent and child navigation through `->parent()` and `->children()`; widget-specific operations require a typed handle. `document()` borrows the owning document. `input()` returns an optional borrowed reference and is empty for programmatic callbacks outside input dispatch. Do not retain the event or its borrowed references after invocation; retain a handle or weak handle when needed.

Callbacks without context, such as `[](bool checked) { ... }`, are convenient when document access is unnecessary. Detection occurs at assignment before type erasure, preferring the context form when a callable accepts both. Running callbacks may safely replace themselves. Context-aware callbacks require a mounted widget; invoking one on a detached widget throws `std::logic_error`. Context-free callbacks can still run detached.

## Flexible sizing

CentUI exposes Yoga 3.2.1 layout through ordinary style values. Flex sizing is a
single value containing `grow`, `shrink`, and `basis`:

```cpp
struct workspace
{
    centui::direction flex_direction = centui::direction::row;
    centui::length gap = centui::px(16);
};

struct sidebar
{
    centui::flex flex = centui::flex::all(0, 0, centui::px(240));
};

struct main_panel
{
    centui::flex flex = centui::flex::all(1, 1, centui::automatic());
    centui::length min_width = centui::px(120);
};
```

`basis` is the starting size along the parent's main axis. `grow` distributes
remaining space; `shrink` distributes a deficit using each child's shrink factor
and basis. Minimum and maximum dimensions constrain the result. The defaults are
`{0, 0, automatic()}`: children do not grow or shrink unless requested.
`flex::all(grow, shrink, basis)` sets all three members. There is no scalar flex
shorthand or hidden precedence between separate flex properties.

Style composition supports partial nested overrides:

```cpp
struct expand_twice
{
    struct
    {
        float grow = 2;
    } flex;
};

auto panel = centui::container<main_panel, expand_twice>{};
panel.patch_style({.flex = centui::flex::all(1, 0, centui::px(200))});
```

Runtime patches replace a supplied nested value as a whole, just as they do for
edges and state colors. To change a single member at runtime while preserving
the others, copy `panel.style().flex`, edit it, and patch the resulting value.

## Lengths, constraints, and edges

- `px(value)` uses logical display units; `percent(value)` uses Yoga's percentage
  reference rules, not a fraction between zero and one.
- `automatic()` requests automatic width, height, flex basis, margin, or inset.
  It replaces the former `fit()` spelling and does not mean CSS `fit-content`.
- `undefined()` clears an edge override or size constraint. It is distinct from
  automatic margins, which can consume free space.
- `min_width`, `min_height`, `max_width`, and `max_height` accept points,
  percentages, or undefined. `aspect_ratio` is an optional positive width/height
  ratio. Minimums take precedence when constraints conflict, following Yoga.
- `box_sizing` selects `box_sizing::border_box` (the default) or
  `box_sizing::content_box`.

`padding`, `margin`, `inset`, and `border_width` use `centui::edges`, containing
`top`, `right`, `bottom`, `left`, `start`, `end`, `horizontal`, and `vertical`.
Entries default to undefined. `edges::all(value)` fills the four physical edges.
Logical start/end override physical left/right, which override horizontal;
physical top/bottom override vertical. To let an axis fallback apply after
`edges::all(...)`, clear the relevant physical entries with `undefined()`.

```cpp
struct padded
{
    centui::edges padding{
        .start = centui::px(24),
        .horizontal = centui::px(12),
        .vertical = centui::px(8),
    };
    centui::edges border_width{.bottom = centui::px(2)};
};
```

Padding and borders must be nonnegative; margins and insets may be negative.
Borders accept logical points or undefined, never percentages or automatic.
Padding accepts points, percentages, or undefined. Invalid values, nonfinite
numbers, and invalid enum choices throw before a widget's style is changed.

Clear constraints and overrides explicitly:

```cpp
panel.patch_style({.max_width = centui::undefined()});
panel.patch_style({.row_gap = centui::undefined()});
panel.patch_style({.aspect_ratio = std::optional<float>{}});
```

An omitted patch field leaves its value unchanged. For optional properties such
as `aspect_ratio`, supplying an empty inner optional clears the value.

## Flow, alignment, and positioning

| Property | Values / behavior |
| --- | --- |
| `flex_direction` | Column, row, and their reversed variants |
| `flex_wrap` | `wrapping::none`, `wrap`, or `reverse` |
| `justify_content` | Start, center, end, space between, around, or evenly |
| `align_items` | Start, center, end, stretch, or baseline |
| `align_self` | The item alignment choices, plus `automatic` to use the parent's alignment |
| `align_content` | Start, center, end, stretch, space between, around, or evenly for wrapped lines |
| `gap` | Default spacing between rows and columns |
| `row_gap`, `column_gap` | Axis-specific overrides; undefined restores `gap` |
| `layout_direction` | `layout_direction::inherit`, `ltr`, or `rtl` |
| `position` | `position::normal` (Yoga static), `relative`, or `absolute` |
| `forms_containing_block` | Makes a static node establish an absolute-positioning containing block |
| `reference_baseline` | Selects a descendant as a container's reference baseline |

Gaps accept nonnegative points or percentages. Relative positioning offsets a
box without changing its allocated space; static positioning ignores its own
insets; absolute positioning removes a box from flex flow. Logical start/end
follow layout direction. RTL layout does not itself change text alignment or
provide a new text-shaping policy.

Text and input widgets report their first-line font baseline. Custom widgets can
supply their own [baseline callback](extending.md). Containers without a custom
callback use Yoga's descendant baseline selection.

## Display and overflow

`display::flex` generates a box. `display::none` removes the widget and its
descendants from layout, painting, pointer targeting, and focus.
`display::contents` preserves widget ownership and event ancestry, while its
children participate in the nearest ancestor's box layout. The wrapper has zero
bounds and no paint, clipping, child offset, pointer target, or focus target.
Its enabled state, visibility, text-style ancestry, and event bubbling remain
meaningful. A contents root places its children in the viewport's default column
layout; its own box styles are ignored.

`visibility::hidden` retains layout space but hides the subtree;
`visibility::collapsed` removes its layout space even when display is contents.

`overflow::visible` permits descendants outside a box. `overflow::clip` clips
them to its content bounds. `overflow::scroll` also selects Yoga's scroll
measurement behavior; use the `scroll` widget for actual scrolling and offsets.
Clipping applies to pointer targeting as well as drawing.

## Layout configuration and results

```cpp
document.set_layout_options({
    .root_direction = centui::layout_direction::rtl,
    .point_scale_factor = 0,
});

const auto& metrics = panel->metrics();
// metrics.margin, padding, and border contain resolved physical edges.
// metrics.direction and metrics.had_overflow report Yoga's layout result.
```

Options apply to all nodes on the next preparation. The default point scale of
zero disables layout rounding; a positive value specifies pixels per logical
unit for Yoga's rounding grid. It is independent of font rasterization scale.
Keep zero unless rounded geometry is wanted.

The three explicit compatibility switches are `stretch_flex_basis`,
`absolute_position_without_insets_excludes_padding`, and
`absolute_percent_against_inner_size`. All default to false. The
`experimental_web_flex_basis` switch is also off by default. CentUI sets its own
style defaults explicitly instead of exposing Yoga's `UseWebDefaults` switch.

`bounds()`, `content_bounds()`, and `metrics()` reflect the last completed
preparation. Yoga's node ownership, cloning, dirty flags, and callback context
are managed internally. No native Yoga handles are needed by widgets.

The [Yoga coverage matrix](yoga-coverage.md) maps supported capabilities to tests.

## Keep a handle when you need it

Copies of a handle refer to the same widget. A parent owns its children, and a document mounts one root. Keep a named handle for a widget you plan to update. You can remove or append children through widget methods or the document; edits requested inside callbacks settle before the next input event.

If a callback must refer to its own widget or an ancestor, capture `weak()` and check `lock()` before using it. A strong capture there would keep both sides alive. For example, the [Studio popup](https://github.com/swordfatih/centui/blob/main/examples/gui_static/main.cpp) closes through a weak handle.
