#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <variant>

#include <centui/centui.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

centui::style box(float width, float height)
{
    centui::style result;
    result.width = centui::px(width);
    result.height = centui::px(height);
    return result;
}

centui::style row(float width = 300, float height = 100)
{
    auto result = box(width, height);
    result.flex_direction = centui::direction::row;
    result.align_items = centui::alignment::start;
    return result;
}

class baseline_widget final : public centui::widget
{
public:
    baseline_widget(float offset, centui::style appearance)
        : widget(appearance),
          m_offset(offset)
    {
    }

    bool fail = false;
    bool mutate = false;

protected:
    centui::baseline_function baseline() override
    {
        return [this](centui::size)
        {
            if(mutate)
            {
                patch_style({.width = centui::px(99)});
            }
            if(fail)
            {
                throw std::runtime_error("baseline failure");
            }
            return m_offset;
        };
    }

private:
    float m_offset;
};

class event_container final : public centui::widget
{
public:
    explicit event_container(centui::style appearance)
        : widget(appearance)
    {
        set_focusable();
        set_interactive();
        set_clip_children(true);
        set_child_offset({50, 50});
    }

    int keys = 0;

protected:
    bool on_event(const centui::input_event& event) override
    {
        if(event.type == centui::input_event_type::key_pressed)
        {
            ++keys;
        }
        return false;
    }
};

} // namespace

TEST_CASE("Flex values compose by member and patch as complete values", "[yoga][style]")
{
    struct base
    {
        centui::flex flex = centui::flex::all(1, 2, centui::percent(30));
    };

    struct override
    {
        struct
        {
            float grow = 3;
        } flex;
    };

    const auto value = centui::style::resolve<base, override>();
    REQUIRE(value.flex == centui::flex::all(3, 2, centui::percent(30)));
    const auto replaced = centui::style_patch{.flex = centui::flex::all(0, 1, centui::automatic())}.apply(value);
    REQUIRE(replaced.flex == centui::flex::all(0, 1, centui::automatic()));
    REQUIRE(replaced.changes_from(value).layout);
    REQUIRE(replaced.changes_from(value).paint);
    REQUIRE_FALSE(value.changes_from(value).any());
}

TEST_CASE("Flex grow distributes free space and respects maximum size", "[yoga][flex]")
{
    centui::layout parent, first, second;
    parent.update(row());
    auto a = box(0, 20);
    a.flex = centui::flex::all(1, 0, centui::px(0));
    auto b = a;
    b.flex.grow = 2;
    first.update(a);
    second.update(b);
    parent.insert_child(first);
    parent.insert_child(second);
    parent.calculate(300, 100);
    REQUIRE(first.bounds().width == Catch::Approx(100));
    REQUIRE(second.bounds().width == Catch::Approx(200));

    a.max_width = centui::px(60);
    first.update(a);
    parent.calculate(300, 100);
    REQUIRE(first.bounds().width == Catch::Approx(60));
    REQUIRE(second.bounds().width == Catch::Approx(240));
}

TEST_CASE("Flex shrink weights the basis and redistributes at minimum sizes", "[yoga][flex]")
{
    centui::layout parent, first, second;
    parent.update(row(150));
    auto a = box(100, 20);
    a.flex.shrink = 1;
    auto b = box(200, 20);
    b.flex.shrink = 1;
    first.update(a);
    second.update(b);
    parent.insert_child(first);
    parent.insert_child(second);
    parent.calculate(150, 100);
    REQUIRE(first.bounds().width == Catch::Approx(50));
    REQUIRE(second.bounds().width == Catch::Approx(100));

    a.min_width = centui::px(80);
    first.update(a);
    parent.calculate(150, 100);
    REQUIRE(first.bounds().width == Catch::Approx(80));
    REQUIRE(second.bounds().width == Catch::Approx(70));

    a.flex.shrink = 0;
    first.update(a);
    parent.calculate(150, 100);
    REQUIRE(first.bounds().width == Catch::Approx(100));
    REQUIRE(second.bounds().width == Catch::Approx(50));
}

TEST_CASE("Basis supports points percent automatic and reset on both axes", "[yoga][flex]")
{
    for(auto direction: {centui::direction::row, centui::direction::column})
    {
        CAPTURE(direction);
        centui::layout parent, child;
        auto outer = box(200, 200);
        outer.flex_direction = direction;
        parent.update(outer);
        auto inner = box(40, 40);
        const auto extent = [&]
        {
            return direction == centui::direction::row ? child.bounds().width : child.bounds().height;
        };
        child.update(inner);
        parent.insert_child(child);
        for(auto basis: {centui::px(60), centui::percent(30), centui::automatic(), centui::undefined()})
        {
            inner.flex.basis = basis;
            child.update(inner);
            parent.calculate(200, 200);
            REQUIRE(
                extent() ==
                Catch::Approx(basis.unit == centui::unit::px || basis.unit == centui::unit::percent ? 60 : 40)
            );
        }
    }
}

TEST_CASE("Size constraints resolve percentages and can be cleared", "[yoga][size]")
{
    centui::layout parent, child;
    parent.update(row(200, 200));
    auto inner = box(20, 180);
    inner.min_width = centui::percent(25);
    inner.max_height = centui::percent(50);
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(200, 200);
    REQUIRE(child.bounds().width == Catch::Approx(50));
    REQUIRE(child.bounds().height == Catch::Approx(100));
    inner.min_width = centui::undefined();
    inner.max_height = centui::undefined();
    inner.max_width = centui::px(10);
    inner.min_height = centui::px(190);
    child.update(inner);
    parent.calculate(200, 200);
    REQUIRE(child.bounds().width == Catch::Approx(10));
    REQUIRE(child.bounds().height == Catch::Approx(190));
    child.update(box(20, 180));
    parent.calculate(200, 200);
    REQUIRE(child.bounds().width == Catch::Approx(20));
    REQUIRE(child.bounds().height == Catch::Approx(180));
}

TEST_CASE("Aspect ratio derives the unspecified dimension and resets", "[yoga][size]")
{
    centui::layout parent, child;
    parent.update(row());
    auto inner = box(80, 0);
    inner.height = centui::automatic();
    inner.aspect_ratio = 2;
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(300, 100);
    REQUIRE(child.bounds().height == Catch::Approx(40));
    inner.width = centui::automatic();
    inner.height = centui::px(30);
    child.update(inner);
    parent.calculate(300, 100);
    REQUIRE(child.bounds().width == Catch::Approx(60));
    inner.aspect_ratio.reset();
    child.update(inner);
    parent.calculate(300, 100);
    REQUIRE(child.bounds().width == Catch::Approx(0));
}

TEST_CASE("Box sizing and per-edge borders agree with content bounds", "[yoga][box]")
{
    centui::layout parent, child;
    parent.update(row());
    auto inner = box(80, 40);
    inner.padding = centui::edges::all(centui::px(5));
    inner.border_width = {centui::px(1), centui::px(2), centui::px(3), centui::px(4)};
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(300, 100);
    REQUIRE(child.content_bounds().width == Catch::Approx(64));
    REQUIRE(child.content_bounds().height == Catch::Approx(26));
    REQUIRE(child.metrics().border == centui::computed_edges{1, 2, 3, 4});
    inner.box_sizing = centui::box_sizing::content_box;
    child.update(inner);
    parent.calculate(300, 100);
    REQUIRE(child.bounds().width == Catch::Approx(96));
    REQUIRE(child.bounds().height == Catch::Approx(54));
    REQUIRE(child.content_bounds().width == Catch::Approx(80));
    REQUIRE(child.content_bounds().height == Catch::Approx(40));
}

TEST_CASE("Wrapping uses independent row and column gaps and align content", "[yoga][wrap]")
{
    centui::layout parent, a, b, c;
    auto outer = row(100, 100);
    outer.flex_wrap = centui::wrapping::wrap;
    outer.gap = centui::px(3);
    outer.row_gap = centui::percent(10);
    outer.column_gap = centui::percent(20);
    parent.update(outer);
    for(auto* child: {&a, &b, &c})
    {
        child->update(box(40, 20));
        parent.insert_child(*child);
    }
    parent.calculate(100, 100);
    REQUIRE(b.bounds().x == Catch::Approx(60));
    REQUIRE(c.bounds().y == Catch::Approx(30));
    outer.align_content = centui::alignment::center;
    parent.update(outer);
    parent.calculate(100, 100);
    REQUIRE(a.bounds().y == Catch::Approx(25));
    REQUIRE(c.bounds().y == Catch::Approx(55));
    outer.align_content = centui::alignment::start;
    outer.flex_wrap = centui::wrapping::reverse;
    parent.update(outer);
    parent.calculate(100, 100);
    REQUIRE(a.bounds().y == Catch::Approx(80));
    REQUIRE(c.bounds().y == Catch::Approx(50));
    outer.flex_wrap = centui::wrapping::none;
    outer.column_gap = centui::undefined();
    parent.update(outer);
    parent.calculate(100, 100);
    REQUIRE(b.bounds().x == Catch::Approx(43));
    REQUIRE(c.bounds().x == Catch::Approx(86));
    REQUIRE(parent.metrics().had_overflow);
}

TEST_CASE("Align self overrides and then inherits align items", "[yoga][alignment]")
{
    centui::layout parent, child;
    parent.update(row(100, 100));
    auto inner = box(20, 20);
    inner.align_self = centui::alignment::end;
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(100, 100);
    REQUIRE(child.bounds().y == Catch::Approx(80));
    inner.align_self = centui::alignment::center;
    child.update(inner);
    parent.calculate(100, 100);
    REQUIRE(child.bounds().y == Catch::Approx(40));
    inner.align_self = centui::alignment::automatic;
    child.update(inner);
    parent.calculate(100, 100);
    REQUIRE(child.bounds().y == Catch::Approx(0));
    inner.height = centui::automatic();
    inner.align_self = centui::alignment::stretch;
    child.update(inner);
    parent.calculate(100, 100);
    REQUIRE(child.bounds().height == Catch::Approx(100));
}

TEST_CASE("RTL inheritance and logical edges resolve to physical metrics", "[yoga][direction]")
{
    centui::layout parent({.root_direction = centui::layout_direction::rtl}), child;
    parent.update(row(200, 100));
    auto inner = box(40, 40);
    inner.margin = {.start = centui::px(10), .end = centui::px(20)};
    inner.padding = {.left = centui::px(1), .start = centui::px(3), .end = centui::px(4), .horizontal = centui::px(2)};
    inner.border_width = {.start = centui::px(5), .end = centui::px(6)};
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(150));
    REQUIRE(child.metrics().direction == centui::layout_direction::rtl);
    REQUIRE(child.metrics().margin.right == Catch::Approx(10));
    REQUIRE(child.metrics().padding.left == Catch::Approx(4));
    REQUIRE(child.metrics().padding.right == Catch::Approx(3));
    REQUIRE(child.metrics().border.left == Catch::Approx(6));
    inner.layout_direction = centui::layout_direction::ltr;
    inner.padding.start = centui::undefined();
    inner.padding.end = centui::undefined();
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.metrics().direction == centui::layout_direction::ltr);
    REQUIRE(child.metrics().padding.left == Catch::Approx(1));
    REQUIRE(child.metrics().padding.right == Catch::Approx(2));
    REQUIRE(child.metrics().border.left == Catch::Approx(5));
}

TEST_CASE("Auto margins absorb free space and negative margins remain valid", "[yoga][edges]")
{
    centui::layout parent, child;
    parent.update(row(200));
    auto inner = box(40, 20);
    inner.margin.left = centui::automatic();
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(160));
    inner.margin.left = centui::px(-10);
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(-10));
    inner.margin = {.horizontal = centui::percent(10), .vertical = centui::px(5)};
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(20));
    REQUIRE(child.bounds().y == Catch::Approx(5));
}

TEST_CASE("Relative static and absolute positioning honor logical and percentage insets", "[yoga][position]")
{
    centui::layout parent, child;
    parent.update(row(200, 100));
    auto inner = box(20, 20);
    inner.inset = {.top = centui::percent(10), .start = centui::percent(25)};
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(50));
    REQUIRE(child.bounds().y == Catch::Approx(10));
    inner.position = centui::position::normal;
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(0));
    REQUIRE(child.bounds().y == Catch::Approx(0));
    inner.position = centui::position::absolute;
    // Yoga resolves absolute logical insets using the parent's direction.
    auto outer = row(200, 100);
    outer.layout_direction = centui::layout_direction::rtl;
    parent.update(outer);
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(130));
    REQUIRE(child.bounds().y == Catch::Approx(10));
    inner.inset = {.bottom = centui::px(5), .end = centui::px(7)};
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(7));
    REQUIRE(child.bounds().y == Catch::Approx(75));
}

TEST_CASE("Static ancestors can explicitly form containing blocks", "[yoga][position]")
{
    auto inner = box(20, 20);
    inner.position = centui::position::absolute;
    inner.inset.left = centui::percent(50);
    auto child = centui::container{inner};
    auto middle_style = box(100, 80);
    middle_style.position = centui::position::normal;
    auto middle = centui::container{{child}, middle_style};
    auto root = centui::container{{middle}, row(200, 100)};
    centui::document document{root};
    (void)document.prepare({200, 100});
    REQUIRE(child->bounds().x == Catch::Approx(100));
    middle.patch_style({.forms_containing_block = true});
    (void)document.prepare({200, 100});
    REQUIRE(child->bounds().x == Catch::Approx(50));
    middle.patch_style({.forms_containing_block = false});
    (void)document.prepare({200, 100});
    REQUIRE(child->bounds().x == Catch::Approx(100));
}

TEST_CASE("Measured content receives flex constraints and automatic basis", "[yoga][measure]")
{
    centui::layout parent, child;
    parent.update(row(100, 100));
    centui::style inner;
    inner.flex.shrink = 1;
    child.update(inner);
    int calls = 0;
    child.set_measure(
        [&](centui::axis_constraint width, centui::axis_constraint)
        {
            ++calls;
            return centui::size{
                width.mode == centui::constraint_mode::unconstrained ? 200.0f : std::min(200.0f, width.value),
                30
            };
        }
    );
    parent.insert_child(child);
    parent.calculate(100, 100);
    child.rethrow_measure_failure();
    REQUIRE(calls > 0);
    REQUIRE(child.bounds().width == Catch::Approx(100));
    REQUIRE(child.bounds().height == Catch::Approx(30));
}

TEST_CASE("Custom baselines include padding and propagate failures safely", "[yoga][baseline]")
{
    auto first_style = box(40, 40);
    first_style.padding.top = centui::px(5);
    first_style.border_width.top = centui::px(2);
    auto first = centui::make_widget<baseline_widget>(10, first_style);
    auto second = centui::make_widget<baseline_widget>(25, box(40, 40));
    auto outer = row();
    outer.align_items = centui::alignment::baseline;
    auto root = centui::container{{first, second}, outer};
    centui::document document{root};
    (void)document.prepare({300, 100});
    REQUIRE(first->bounds().y + 17 == Catch::Approx(second->bounds().y + 25));
    first->fail = true;
    first.patch_style({.width = centui::px(41)});
    REQUIRE_THROWS_AS(document.prepare({300, 100}), std::runtime_error);
    REQUIRE(document.pending_changes().layout);
    first->fail = false;
    REQUIRE_NOTHROW(document.prepare({300, 100}));
    first->mutate = true;
    first.patch_style({.width = centui::px(42)});
    REQUIRE_THROWS_AS(document.prepare({300, 100}), std::logic_error);
    REQUIRE(first.style().width == centui::px(42));
    first->mutate = false;
    REQUIRE_NOTHROW(document.prepare({300, 100}));
}

TEST_CASE("Reference baseline selects a descendant for container alignment", "[yoga][baseline]")
{
    centui::layout root, group, first, second, peer;
    auto outer = row();
    outer.align_items = centui::alignment::baseline;
    root.update(outer);
    group.update(row(80, 40));
    first.update(box(20, 10));
    auto selected = box(20, 30);
    selected.reference_baseline = true;
    second.update(selected);
    peer.update(box(20, 20));
    group.insert_child(first);
    group.insert_child(second);
    root.insert_child(group);
    root.insert_child(peer);
    root.calculate(300, 100);
    REQUIRE(group.bounds().y + 30 == Catch::Approx(peer.bounds().y + 20));
    selected.reference_baseline = false;
    second.update(selected);
    root.calculate(300, 100);
    REQUIRE(group.bounds().y + 10 == Catch::Approx(peer.bounds().y + 20));
}

TEST_CASE("Different text sizes align using font ascenders", "[yoga][baseline][font]")
{
    auto small = centui::text{"Small"};
    auto large = centui::text{"Large"};
    small.patch_style({.font_size = 12});
    large.patch_style({.font_size = 30});
    auto outer = row();
    outer.align_items = centui::alignment::baseline;
    auto root = centui::container{{small, large}, outer};
    centui::document document{root};
    centui::font face{CENTUI_FONT_PATH};
    document.set_font(face);
    (void)document.prepare({300, 100});
    REQUIRE(
        small->content_bounds().y + face.ascender(12) == Catch::Approx(large->content_bounds().y + face.ascender(30))
    );
}

TEST_CASE("Contents wrappers preserve layout painting events and ownership", "[yoga][display]")
{
    int clicks = 0;
    auto child = centui::pressable{
        centui::container{box(30, 20)},
        {.on_click = [&]
         {
             ++clicks;
         }}
    };
    child.patch_style(
        {.width = centui::px(30),
         .height = centui::px(20),
         .background_color = centui::states_color::all({255, 0, 0, 255})}
    );
    auto contents = box(150, 80);
    contents.display = centui::display::contents;
    contents.overflow = centui::overflow::clip;
    contents.padding = centui::edges::all(centui::px(20));
    contents.background_color = centui::states_color::all({0, 255, 0, 255});
    auto wrapper = centui::make_widget<event_container>(contents);
    auto nested = centui::container{{child}, contents};
    wrapper->append(nested);
    auto outer = row(200, 100);
    outer.padding = centui::edges::all(centui::px(10));
    outer.gap = centui::px(5);
    auto before = centui::container{box(20, 20)};
    auto root = centui::container{{before, wrapper}, outer};
    centui::document document{root};
    const auto& drawing = document.prepare({200, 100});
    REQUIRE(child->bounds().x == Catch::Approx(35));
    REQUIRE(child->bounds().y == Catch::Approx(10));
    REQUIRE(wrapper->bounds().width == 0);
    REQUIRE(drawing.commands.size() == 1);
    REQUIRE(child->parent() == centui::handle<>{nested});
    REQUIRE_FALSE(wrapper->request_focus());
    REQUIRE(child->request_focus());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {200, 100});
    REQUIRE(wrapper->keys == 1);
    document.process_event(
        {centui::input_event_type::pointer_pressed, 40, 15, centui::mouse_button::primary},
        {200, 100}
    );
    document.process_event(
        {centui::input_event_type::pointer_released, 40, 15, centui::mouse_button::primary},
        {200, 100}
    );
    REQUIRE(clicks == 1);
    wrapper.patch_style({.display = centui::display::none});
    REQUIRE(document.prepare({200, 100}).commands.empty());
    REQUIRE_FALSE(child->effectively_visible());
    REQUIRE_FALSE(child->focused());
    wrapper.patch_style({.display = centui::display::contents});
    (void)document.prepare({200, 100});
    REQUIRE(child->bounds().x == Catch::Approx(35));
}

TEST_CASE("Hidden retains layout while collapsed and display none remove it", "[yoga][display]")
{
    auto first = centui::container{box(40, 20)};
    auto second = centui::container{box(40, 20)};
    auto root = centui::container{{first, second}, row()};
    centui::document document{root};
    first.patch_style({.visibility = centui::visibility::hidden});
    (void)document.prepare({300, 100});
    REQUIRE(second->bounds().x == Catch::Approx(40));
    first.patch_style({.visibility = centui::visibility::collapsed});
    (void)document.prepare({300, 100});
    REQUIRE(second->bounds().x == Catch::Approx(0));
    first.patch_style({.visibility = centui::visibility::visible, .display = centui::display::none});
    (void)document.prepare({300, 100});
    REQUIRE(second->bounds().x == Catch::Approx(0));
    first.patch_style({.display = centui::display::flex});
    (void)document.prepare({300, 100});
    REQUIRE(second->bounds().x == Catch::Approx(40));
}

TEST_CASE("Border painting uses each resolved physical width", "[yoga][render]")
{
    auto appearance = box(80, 40);
    appearance.border_width = {centui::px(1), centui::px(2), centui::px(3), centui::px(4)};
    appearance.border_color = centui::states_color::all({255, 0, 0, 255});
    auto root = centui::container{appearance};
    centui::document document{root};
    const auto& draw = document.prepare({100, 100});
    REQUIRE(draw.commands.size() == 4);
    REQUIRE(draw.commands[0].bounds.height == 1);
    REQUIRE(draw.commands[1].bounds.height == 3);
    REQUIRE(draw.commands[2].bounds.width == 4);
    REQUIRE(draw.commands[3].bounds.width == 2);
}

TEST_CASE("Document layout options invalidate only when changed and validate atomically", "[yoga][config]")
{
    auto child = centui::container{box(20, 20)};
    auto root = centui::container{{child}, row(100, 100)};
    centui::document document{root};
    (void)document.prepare({100, 100});
    document.set_layout_options({});
    REQUIRE_FALSE(document.pending_changes().any());
    document.set_layout_options({.root_direction = centui::layout_direction::rtl});
    REQUIRE(document.pending_changes().layout);
    (void)document.prepare({100, 100});
    REQUIRE(child->bounds().x == Catch::Approx(80));
    REQUIRE_THROWS_AS(document.set_layout_options({.point_scale_factor = -1}), std::invalid_argument);
    REQUIRE(document.options().root_direction == centui::layout_direction::rtl);
    REQUIRE_THROWS_AS(
        document.set_layout_options({.root_direction = centui::layout_direction::inherit}),
        std::invalid_argument
    );
    REQUIRE_THROWS_AS(
        document.set_layout_options({.point_scale_factor = std::numeric_limits<float>::infinity()}),
        std::invalid_argument
    );
}

TEST_CASE("Point scale controls fractional layout rounding", "[yoga][config]")
{
    for(float scale: {0.0f, 1.0f, 2.0f})
    {
        CAPTURE(scale);
        centui::layout parent({.point_scale_factor = scale}), child({.point_scale_factor = scale});
        parent.update(row(100, 100));
        child.update(box(10.3f, 20));
        parent.insert_child(child);
        parent.calculate(100, 100);
        REQUIRE(child.bounds().width == Catch::Approx(scale == 0 ? 10.3f : scale == 1 ? 10.0f : 10.5f));
    }
}

TEST_CASE("Length and flex validation rejects unsupported units and nonfinite values", "[yoga][validation]")
{
    for(float invalid: {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        centui::style value;
        value.flex.grow = invalid;
        REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
        value.flex = {};
        value.flex.shrink = invalid;
        REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
        value.flex = {};
        value.aspect_ratio = invalid;
        REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    }
    centui::style value;
    value.padding.start = centui::automatic();
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.border_width.end = centui::percent(1);
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.min_width = centui::automatic();
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.row_gap = centui::automatic();
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.flex.basis = centui::px(-1);
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.width = {1, centui::unit::automatic};
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.aspect_ratio = 0;
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.align_items = centui::alignment::space_between;
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.align_self = centui::alignment::space_around;
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
    value = {};
    value.align_content = centui::alignment::baseline;
    REQUIRE_THROWS_AS(value.validate(), std::invalid_argument);
}

TEST_CASE("Every flex direction places children on its main axis", "[yoga][direction]")
{
    struct scenario
    {
        centui::direction direction;
        float x;
        float y;
    };

    for(auto test:
        {scenario{centui::direction::row, 0, 0},
         scenario{centui::direction::row_reverse, 80, 0},
         scenario{centui::direction::column, 0, 0},
         scenario{centui::direction::column_reverse, 0, 80}})
    {
        CAPTURE(test.direction);
        centui::layout parent, child;
        auto outer = row(100, 100);
        outer.flex_direction = test.direction;
        parent.update(outer);
        child.update(box(20, 20));
        parent.insert_child(child);
        parent.calculate(100, 100);
        REQUIRE(child.bounds().x == Catch::Approx(test.x));
        REQUIRE(child.bounds().y == Catch::Approx(test.y));
    }
}

TEST_CASE("Every justification distributes space between children", "[yoga][alignment]")
{
    struct scenario
    {
        centui::justification justify;
        float first;
        float second;
    };

    for(auto test:
        {scenario{centui::justification::start, 0, 20},
         scenario{centui::justification::center, 30, 50},
         scenario{centui::justification::end, 60, 80},
         scenario{centui::justification::space_between, 0, 80},
         scenario{centui::justification::space_around, 15, 65},
         scenario{centui::justification::space_evenly, 20, 60}})
    {
        CAPTURE(test.justify);
        centui::layout parent, first, second;
        auto outer = row(100, 100);
        outer.justify_content = test.justify;
        parent.update(outer);
        first.update(box(20, 20));
        second.update(box(20, 20));
        parent.insert_child(first);
        parent.insert_child(second);
        parent.calculate(100, 100);
        REQUIRE(first.bounds().x == Catch::Approx(test.first));
        REQUIRE(second.bounds().x == Catch::Approx(test.second));
    }
}

TEST_CASE("Every content alignment positions wrapped lines", "[yoga][alignment]")
{
    struct scenario
    {
        centui::alignment align;
        float first;
        float second;
    };

    for(auto test:
        {scenario{centui::alignment::start, 0, 20},
         scenario{centui::alignment::center, 30, 50},
         scenario{centui::alignment::end, 60, 80},
         scenario{centui::alignment::stretch, 0, 50},
         scenario{centui::alignment::space_between, 0, 80},
         scenario{centui::alignment::space_around, 15, 65},
         scenario{centui::alignment::space_evenly, 20, 60}})
    {
        CAPTURE(test.align);
        centui::layout parent, first, second;
        auto outer = row(100, 100);
        outer.flex_wrap = centui::wrapping::wrap;
        outer.align_content = test.align;
        parent.update(outer);
        first.update(box(60, 20));
        second.update(box(60, 20));
        parent.insert_child(first);
        parent.insert_child(second);
        parent.calculate(100, 100);
        REQUIRE(first.bounds().y == Catch::Approx(test.first));
        REQUIRE(second.bounds().y == Catch::Approx(test.second));
    }
}

TEST_CASE("Overflow modes control clipping and pointer targets", "[yoga][overflow]")
{
    for(auto mode: {centui::overflow::visible, centui::overflow::clip, centui::overflow::scroll})
    {
        CAPTURE(mode);
        int clicks = 0;
        auto child = centui::pressable{
            centui::container{box(80, 20)},
            {.on_click = [&]
             {
                 ++clicks;
             }}
        };
        child.patch_style(
            {.width = centui::px(80),
             .height = centui::px(20),
             .background_color = centui::states_color::all({255, 0, 0, 255})}
        );
        auto appearance = row(40, 40);
        appearance.overflow = mode;
        auto root = centui::container{{child}, appearance};
        centui::document document{root};
        const auto& draw = document.prepare({100, 100});
        REQUIRE(root->metrics().had_overflow);
        REQUIRE(draw.commands.size() == 1);
        const auto clip = draw.commands.front().bounds;
        REQUIRE(clip.width == Catch::Approx(mode == centui::overflow::visible ? 80 : 40));
        document.process_event(
            {centui::input_event_type::pointer_pressed, 60, 10, centui::mouse_button::primary},
            {100, 100}
        );
        document.process_event(
            {centui::input_event_type::pointer_released, 60, 10, centui::mouse_button::primary},
            {100, 100}
        );
        REQUIRE(clicks == (mode == centui::overflow::visible ? 1 : 0));
    }
}

TEST_CASE("A contents root uses the viewport and ignores its own box styles", "[yoga][display]")
{
    auto child = centui::container{box(30, 20)};
    auto appearance = row(10, 10);
    appearance.display = centui::display::contents;
    appearance.padding = centui::edges::all(centui::px(100));
    auto root = centui::container{{child}, appearance};
    centui::document document{root};
    (void)document.prepare({200, 100});
    REQUIRE(root->bounds().width == 0);
    REQUIRE(child->bounds().x == 0);
    REQUIRE(child->bounds().y == 0);
    REQUIRE(child->bounds().width == 30);
    root.patch_style({.visibility = centui::visibility::collapsed});
    (void)document.prepare({200, 100});
    REQUIRE(child->bounds().width == 0);
}

TEST_CASE("Compatibility options control absolute padding and percentage sizing", "[yoga][config]")
{
    for(bool legacy_padding: {false, true})
    {
        for(bool legacy_percent: {false, true})
        {
            CAPTURE(legacy_padding, legacy_percent);
            centui::layout_options options;
            options.absolute_position_without_insets_excludes_padding = legacy_padding;
            options.absolute_percent_against_inner_size = legacy_percent;
            centui::layout parent(options), child(options);
            auto outer = row(200, 100);
            outer.padding = centui::edges::all(centui::px(20));
            parent.update(outer);
            auto inner = box(0, 20);
            inner.width = centui::percent(50);
            inner.position = centui::position::absolute;
            child.update(inner);
            parent.insert_child(child);
            parent.calculate(200, 100);
            REQUIRE(child.bounds().x == Catch::Approx(legacy_padding ? 0 : 20));
            REQUIRE(child.bounds().y == Catch::Approx(legacy_padding ? 0 : 20));
            REQUIRE(child.bounds().width == Catch::Approx(legacy_percent ? 80 : 100));
        }
    }
}

TEST_CASE("Stretch flex basis compatibility changes intrinsic column height", "[yoga][config]")
{
    for(bool legacy: {false, true})
    {
        centui::layout_options options;
        options.stretch_flex_basis = legacy;
        centui::layout root(options), child(options), grandchild(options), leaf(options);
        root.update(box(500, 500));
        centui::style appearance;
        appearance.align_items = centui::alignment::start;
        child.update(appearance);
        appearance = {};
        appearance.flex = centui::flex::all(1, 1, centui::automatic());
        grandchild.update(appearance);
        leaf.update(appearance);
        grandchild.insert_child(leaf);
        child.insert_child(grandchild);
        root.insert_child(child);
        root.calculate(500, 500);
        REQUIRE(child.bounds().height == Catch::Approx(legacy ? 500 : 0));
    }
}

TEST_CASE("Experimental web basis recalculates percentages when available size changes", "[yoga][config]")
{
    centui::layout_options options;
    options.experimental_web_flex_basis = true;
    centui::layout parent(options), child(options);
    centui::style outer;
    outer.width = centui::percent(100);
    outer.height = centui::percent(100);
    parent.update(outer);
    centui::style inner;
    inner.flex.basis = centui::percent(100);
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(100, std::numeric_limits<float>::quiet_NaN());
    parent.calculate(100, 100);
    REQUIRE(child.bounds().height == Catch::Approx(100));
}

TEST_CASE("New layout properties compose and participate in patch invalidation", "[yoga][style]")
{
    struct properties
    {
        centui::length min_width = centui::px(1);
        centui::length min_height = centui::px(2);
        centui::length max_width = centui::px(100);
        centui::length max_height = centui::px(200);
        std::optional<float> aspect_ratio = 2;
        centui::wrapping flex_wrap = centui::wrapping::wrap;
        centui::alignment align_self = centui::alignment::end;
        centui::alignment align_content = centui::alignment::center;
        centui::length row_gap = centui::px(3);
        centui::length column_gap = centui::px(4);
        centui::layout_direction layout_direction = centui::layout_direction::rtl;
        centui::box_sizing box_sizing = centui::box_sizing::content_box;
        centui::display display = centui::display::contents;
        bool reference_baseline = true;
        bool forms_containing_block = true;

        struct
        {
            centui::length start = centui::px(5);
            centui::length horizontal = centui::px(6);
        } padding;
    };

    const auto composed = centui::style::resolve<properties>();
    const centui::style initial;
    const std::array patches{
        centui::style_patch{.min_width = centui::px(1)},
        centui::style_patch{.min_height = centui::px(2)},
        centui::style_patch{.max_width = centui::px(100)},
        centui::style_patch{.max_height = centui::px(200)},
        centui::style_patch{.aspect_ratio = std::optional<float>{2}},
        centui::style_patch{.flex_wrap = centui::wrapping::wrap},
        centui::style_patch{.align_self = centui::alignment::end},
        centui::style_patch{.align_content = centui::alignment::center},
        centui::style_patch{.row_gap = centui::px(3)},
        centui::style_patch{.column_gap = centui::px(4)},
        centui::style_patch{.layout_direction = centui::layout_direction::rtl},
        centui::style_patch{.box_sizing = centui::box_sizing::content_box},
        centui::style_patch{.display = centui::display::contents},
        centui::style_patch{.reference_baseline = true},
        centui::style_patch{.forms_containing_block = true},
        centui::style_patch{.padding = centui::edges{.start = centui::px(5), .horizontal = centui::px(6)}}
    };
    auto combined = initial;
    for(const auto& patch: patches)
    {
        const auto changed = patch.apply(initial);
        REQUIRE(changed.changes_from(initial).layout);
        REQUIRE(changed.changes_from(initial).paint);
        REQUIRE_FALSE(patch.apply(changed).changes_from(changed).any());
        combined = patch.apply(combined);
    }
    REQUIRE(composed == combined);
    const auto cleared = centui::style_patch{.aspect_ratio = std::optional<float>{}}.apply(composed);
    REQUIRE_FALSE(cleared.aspect_ratio);
    REQUIRE(cleared.changes_from(composed).layout);
}

TEST_CASE("Invalid layout patches leave mounted style and invalidation unchanged", "[yoga][validation]")
{
    auto root = centui::container{box(100, 100)};
    centui::document document{root};
    (void)document.prepare({100, 100});
    const auto before = root.style();
    REQUIRE_THROWS_AS(
        root.patch_style({.width = centui::px(50), .flex = centui::flex::all(-1, 1, centui::automatic())}),
        std::invalid_argument
    );
    REQUIRE(root.style() == before);
    REQUIRE_FALSE(document.pending_changes().any());
}

TEST_CASE("Baseline and measurement failures do not unwind through Yoga", "[yoga][measure]")
{
    centui::layout parent, child;
    auto outer = row(100, 100);
    outer.align_items = centui::alignment::baseline;
    parent.update(outer);
    child.update({});
    child.set_measure(
        [](centui::axis_constraint, centui::axis_constraint)
        {
            return centui::size{-1, 10};
        }
    );
    parent.insert_child(child);
    REQUIRE_NOTHROW(parent.calculate(100, 100));
    REQUIRE_THROWS_AS(child.rethrow_measure_failure(), std::invalid_argument);
    child.set_measure(
        [](centui::axis_constraint, centui::axis_constraint)
        {
            return centui::size{10, 10};
        }
    );
    child.set_baseline(
        [](centui::size)
        {
            return std::numeric_limits<float>::quiet_NaN();
        }
    );
    REQUIRE_NOTHROW(parent.calculate(100, 100));
    REQUIRE_THROWS_AS(child.rethrow_measure_failure(), std::invalid_argument);
}

TEST_CASE("All item alignments resolve the cross axis", "[yoga][alignment]")
{
    for(auto alignment:
        {centui::alignment::start, centui::alignment::center, centui::alignment::end, centui::alignment::stretch})
    {
        CAPTURE(alignment);
        centui::layout parent, child;
        auto outer = row(100, 100);
        outer.align_items = alignment;
        parent.update(outer);
        auto inner = box(20, 20);
        if(alignment == centui::alignment::stretch)
        {
            inner.height = centui::automatic();
        }
        child.update(inner);
        parent.insert_child(child);
        parent.calculate(100, 100);
        REQUIRE(
            child.bounds().y == Catch::Approx(
                                    alignment == centui::alignment::center ? 40
                                    : alignment == centui::alignment::end  ? 80
                                                                           : 0
                                )
        );
        REQUIRE(child.bounds().height == Catch::Approx(alignment == centui::alignment::stretch ? 100 : 20));
    }
}

TEST_CASE("Dimensions and padding resolve percentages against their containing box", "[yoga][size]")
{
    centui::layout parent, child;
    parent.update(row(200, 100));
    centui::style inner;
    inner.width = centui::percent(50);
    inner.height = centui::percent(50);
    inner.padding = centui::edges::all(centui::percent(5));
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().width == Catch::Approx(100));
    REQUIRE(child.bounds().height == Catch::Approx(50));
    REQUIRE(child.metrics().padding == centui::computed_edges{10, 10, 10, 10});
    REQUIRE(child.content_bounds().width == Catch::Approx(80));
    REQUIRE(child.content_bounds().height == Catch::Approx(30));
    inner.width = centui::undefined();
    inner.height = centui::undefined();
    inner.padding = {};
    child.update(inner);
    parent.calculate(200, 100);
    REQUIRE(child.bounds().width == 0);
    REQUIRE(child.bounds().height == 0);
}

TEST_CASE("Automatic insets restore normal absolute alignment", "[yoga][position]")
{
    centui::layout parent, child;
    auto outer = row(100, 100);
    outer.justify_content = centui::justification::center;
    outer.align_items = centui::alignment::center;
    parent.update(outer);
    auto inner = box(20, 20);
    inner.position = centui::position::absolute;
    inner.inset = centui::edges::all(centui::px(10));
    child.update(inner);
    parent.insert_child(child);
    parent.calculate(100, 100);
    REQUIRE(child.bounds().x == Catch::Approx(10));
    inner.inset = centui::edges::all(centui::automatic());
    child.update(inner);
    parent.calculate(100, 100);
    REQUIRE(child.bounds().x == Catch::Approx(40));
    REQUIRE(child.bounds().y == Catch::Approx(40));
}

TEST_CASE("Text inputs share the first-line baseline with text labels", "[yoga][baseline][font]")
{
    auto label = centui::text{"Label"};
    auto input = centui::input{"Value"};
    label.patch_style({.font_size = 12});
    input.patch_style({.padding = centui::edges::all(centui::px(4)), .font_size = 24});
    auto outer = row(400, 100);
    outer.align_items = centui::alignment::baseline;
    auto root = centui::container{{label, input}, outer};
    centui::document document{root};
    centui::font face{CENTUI_FONT_PATH};
    document.set_font(face);
    (void)document.prepare({400, 100});
    REQUIRE(
        label->content_bounds().y + face.ascender(12) == Catch::Approx(input->content_bounds().y + face.ascender(24))
    );
}

TEST_CASE("Invalid enum values and every numeric length are rejected before Yoga", "[yoga][validation]")
{
    for(auto member:
        {&centui::style::width,
         &centui::style::height,
         &centui::style::min_width,
         &centui::style::min_height,
         &centui::style::max_width,
         &centui::style::max_height,
         &centui::style::gap,
         &centui::style::row_gap,
         &centui::style::column_gap})
    {
        for(auto value:
            {centui::px(-1),
             centui::percent(-1),
             centui::px(std::numeric_limits<float>::infinity()),
             centui::length{0, static_cast<centui::unit>(99)}})
        {
            centui::style appearance;
            appearance.*member = value;
            REQUIRE_THROWS_AS(appearance.validate(), std::invalid_argument);
        }
    }
    const std::array patches{
        centui::style_patch{.flex_direction = static_cast<centui::direction>(99)},
        centui::style_patch{.align_items = static_cast<centui::alignment>(99)},
        centui::style_patch{.justify_content = static_cast<centui::justification>(99)},
        centui::style_patch{.position = static_cast<centui::position>(99)},
        centui::style_patch{.overflow = static_cast<centui::overflow>(99)},
        centui::style_patch{.flex_wrap = static_cast<centui::wrapping>(99)},
        centui::style_patch{.align_self = static_cast<centui::alignment>(99)},
        centui::style_patch{.align_content = static_cast<centui::alignment>(99)},
        centui::style_patch{.layout_direction = static_cast<centui::layout_direction>(99)},
        centui::style_patch{.box_sizing = static_cast<centui::box_sizing>(99)},
        centui::style_patch{.display = static_cast<centui::display>(99)}
    };
    for(const auto& patch: patches)
    {
        REQUIRE_THROWS_AS(patch.apply({}), std::invalid_argument);
    }
    for(auto member:
        {&centui::edges::top,
         &centui::edges::right,
         &centui::edges::bottom,
         &centui::edges::left,
         &centui::edges::start,
         &centui::edges::end,
         &centui::edges::horizontal,
         &centui::edges::vertical})
    {
        centui::style appearance;
        appearance.padding.*member = centui::px(-1);
        REQUIRE_THROWS_AS(appearance.validate(), std::invalid_argument);
        appearance = {};
        appearance.border_width.*member = centui::automatic();
        REQUIRE_THROWS_AS(appearance.validate(), std::invalid_argument);
        appearance = {};
        appearance.margin.*member = centui::px(std::numeric_limits<float>::quiet_NaN());
        REQUIRE_THROWS_AS(appearance.validate(), std::invalid_argument);
        appearance = {};
        appearance.inset.*member = {1, centui::unit::undefined};
        REQUIRE_THROWS_AS(appearance.validate(), std::invalid_argument);
    }
}

TEST_CASE("Every minimum and maximum dimension supports points and percentages", "[yoga][size]")
{
    struct constraint
    {
        centui::length centui::style::* member;
        bool horizontal;
        bool minimum;
    };

    for(auto test:
        {constraint{&centui::style::min_width, true, true},
         constraint{&centui::style::min_height, false, true},
         constraint{&centui::style::max_width, true, false},
         constraint{&centui::style::max_height, false, false}})
    {
        for(auto value: {centui::px(50), centui::percent(25)})
        {
            CAPTURE(test.horizontal, test.minimum, value.unit);
            centui::layout parent, child;
            parent.update(row(200, 200));
            auto inner = test.minimum ? box(20, 20) : box(180, 180);
            inner.*test.member = value;
            child.update(inner);
            parent.insert_child(child);
            parent.calculate(200, 200);
            REQUIRE((test.horizontal ? child.bounds().width : child.bounds().height) == Catch::Approx(50));
        }
    }
}

TEST_CASE("Mounted flex patches resize siblings and reset growth", "[yoga][flex]")
{
    auto flexible = box(0, 20);
    flexible.flex = centui::flex::all(1, 1, centui::px(0));
    auto first = centui::container{flexible};
    auto second = centui::container{flexible};
    auto root = centui::container{{first, second}, row(300, 100)};
    centui::document document{root};
    (void)document.prepare({300, 100});
    REQUIRE(first->bounds().width == Catch::Approx(150));
    first.patch_style({.flex = centui::flex::all(2, 1, centui::px(0))});
    REQUIRE(document.pending_changes().layout);
    (void)document.prepare({300, 100});
    REQUIRE(first->bounds().width == Catch::Approx(200));
    REQUIRE(second->bounds().width == Catch::Approx(100));
    first.patch_style({.flex = centui::flex{}});
    (void)document.prepare({300, 100});
    REQUIRE(first->bounds().width == Catch::Approx(0));
    REQUIRE(second->bounds().width == Catch::Approx(300));
}

TEST_CASE("Changing container baseline callbacks invalidates cached layout", "[yoga][baseline]")
{
    centui::layout parent, group, child, peer;
    auto outer = row(200, 100);
    outer.align_items = centui::alignment::baseline;
    parent.update(outer);
    group.update(box(40, 40));
    child.update(box(10, 10));
    peer.update(box(20, 20));
    group.insert_child(child);
    parent.insert_child(group);
    parent.insert_child(peer);
    group.set_baseline(
        [](centui::size)
        {
            return 30;
        }
    );
    parent.calculate(200, 100);
    REQUIRE(peer.bounds().y == Catch::Approx(10));
    group.set_baseline(
        [](centui::size)
        {
            return 25;
        }
    );
    parent.calculate(200, 100);
    REQUIRE(peer.bounds().y == Catch::Approx(5));
    group.set_baseline({});
    parent.calculate(200, 100);
    REQUIRE(group.bounds().y == Catch::Approx(10));
    REQUIRE(peer.bounds().y == Catch::Approx(0));
}

TEST_CASE("Containing-block metadata invalidates a reused Yoga tree", "[yoga][position]")
{
    centui::layout root, middle, child;
    root.update(row(200, 100));
    auto middle_style = box(100, 80);
    middle_style.position = centui::position::normal;
    middle.update(middle_style);
    auto inner = box(20, 20);
    inner.position = centui::position::absolute;
    inner.inset.left = centui::percent(50);
    child.update(inner);
    middle.insert_child(child);
    root.insert_child(middle);
    root.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(100));
    middle_style.forms_containing_block = true;
    middle.update(middle_style);
    root.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(50));
    middle_style.forms_containing_block = false;
    middle.update(middle_style);
    root.calculate(200, 100);
    REQUIRE(child.bounds().x == Catch::Approx(100));
}
