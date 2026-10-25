#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class probe final : public centui::widget
{
public:
    float natural_width = 20;
    int geometries = 0;
    mutable int paints = 0;
    bool fail_measure = false;
    bool fail_baseline = false;
    bool fail_paint = false;
    std::function<void()> geometry;

    void metrics_changed()
    {
        invalidate_layout();
    }

    void geometry_changed()
    {
        invalidate_geometry();
    }

    void drawing_changed()
    {
        invalidate_paint();
    }

    void interactive(bool value)
    {
        set_interactive(value);
    }

    void focusable(bool value)
    {
        set_focusable(value);
    }

    void offset(centui::vec2 value)
    {
        set_child_offset(value);
    }

    void clipping(bool value)
    {
        set_clip_children(value);
    }

protected:
    centui::size measure(const centui::measure_context&) override
    {
        if(fail_measure)
        {
            throw std::runtime_error("measure");
        }
        return {natural_width, 20};
    }

    centui::baseline_function baseline() override
    {
        return [this](centui::size)
        {
            if(fail_baseline)
            {
                throw std::runtime_error("baseline");
            }
            return 10.0f;
        };
    }

    void on_geometry() override
    {
        ++geometries;
        if(geometry)
        {
            geometry();
        }
    }

    void paint(centui::painter& output) const override
    {
        ++paints;
        if(fail_paint)
        {
            throw std::runtime_error("paint");
        }
        output.rectangle(output.bounds(), output.foreground());
    }
};

// Compare semantic commands, deliberately excluding resource pointer identity.
void same_drawing(const centui::draw_list& actual, const centui::draw_list& expected)
{
    REQUIRE(actual.viewport == expected.viewport);
    REQUIRE(actual.commands.size() == expected.commands.size());
    for(std::size_t i = 0; i < actual.commands.size(); ++i)
    {
        const auto& a = actual.commands[i];
        const auto& b = expected.commands[i];
        REQUIRE(a.bounds == b.bounds);
        REQUIRE(a.color == b.color);
        REQUIRE(a.uv == b.uv);
        REQUIRE(a.sampling == b.sampling);
        REQUIRE(static_cast<bool>(a.image) == static_cast<bool>(b.image));
        if(a.image && b.image)
        {
            REQUIRE(a.image->width == b.image->width);
            REQUIRE(a.image->height == b.image->height);
            REQUIRE(a.image->pixels == b.image->pixels);
        }
    }
}

} // namespace

TEST_CASE("Local metrics synchronize one branch and retain clean native inputs", "[invalidation][layout]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    auto left = centui::container{a};
    auto right = centui::container{b};
    auto root = centui::container{left, right};
    root->patch_style({.flex_direction = centui::direction::row, .align_items = centui::alignment::start});
    centui::document document{root};
    (void)document.prepare({200, 100});
    auto before = document.statistics();
    (void)document.prepare({200, 100});
    a->set_style(a->style());
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().layout_nodes_synchronized == before.layout_nodes_synchronized);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    REQUIRE(document.statistics().paint_visits == before.paint_visits);

    a->natural_width = 50;
    a->metrics_changed();
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().layout_nodes_synchronized == before.layout_nodes_synchronized + 1);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations + 1);
    REQUIRE(right->bounds().x == 50);
    REQUIRE(b->bounds().x == 50);
}

TEST_CASE("Explicit geometry coalesces, includes paint, and preserves sibling boundaries", "[invalidation][geometry]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    a->set_paint_boundary(true);
    b->set_paint_boundary(true);
    auto root = centui::container{a, b};
    root->set_paint_boundary(true);
    centui::document document{root};
    (void)document.prepare({200, 100});
    const auto before = document.statistics();
    const auto b_geometries = b->geometries;
    a->geometry_changed();
    a->geometry_changed();
    REQUIRE_FALSE(document.pending_changes().layout);
    REQUIRE(document.pending_changes().paint);
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().geometry_callbacks == before.geometry_callbacks + 1);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    REQUIRE(document.statistics().measurement_calls == before.measurement_calls);
    REQUIRE(b->geometries == b_geometries);
    REQUIRE(a->paints == 2);
    REQUIRE(b->paints == 1);
    REQUIRE_FALSE(document.pending_changes().any());
}

TEST_CASE("Geometry retries unfinished hooks and requests made by hooks", "[invalidation][geometry]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    centui::document document{centui::container{a, b}};
    (void)document.prepare({200, 100});
    const auto before = document.statistics();
    a->geometry = []
    {
        throw std::runtime_error("geometry");
    };
    a->geometry_changed();
    b->geometry_changed();
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::runtime_error);
    REQUIRE(document.pending_changes().paint);
    REQUIRE(b->geometries == 1);
    int attempts = 0;
    a->geometry = [&]
    {
        if(++attempts == 1)
        {
            a->geometry_changed();
        }
    };
    (void)document.prepare({200, 100});
    REQUIRE(attempts == 2);
    REQUIRE(b->geometries == 2);
    REQUIRE(document.statistics().geometry_callbacks == before.geometry_callbacks + 4);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
}

TEST_CASE("Geometry hooks settle offsets before descendant geometry and targeting", "[invalidation][geometry]")
{
    auto child = centui::make_widget<probe>();
    child->interactive(true);
    auto root = centui::make_widget<probe>();
    root->append(child);
    root->patch_style({.width = centui::px(100), .height = centui::px(100)});
    centui::document document{root};
    (void)document.prepare({200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 5, 5}, {200, 100});
    REQUIRE(child->hovered());
    const auto before = document.statistics();
    root->geometry = [&]
    {
        root->offset({40, 0});
    };
    root->geometry_changed();
    child->geometry = [&]
    {
        REQUIRE(child->bounds().x == 40);
    };
    (void)document.prepare({200, 100});
    REQUIRE(child->bounds().x == 40);
    REQUIRE_FALSE(child->hovered());
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    REQUIRE_FALSE(document.pending_changes().any());
}

TEST_CASE("Detached geometry survives mounting and removal releases pending widgets", "[invalidation][geometry]")
{
    auto child = centui::make_widget<probe>();
    child->geometry_changed();
    auto root = centui::container{};
    centui::document first{root};
    first.append(root, child);
    (void)first.prepare({200, 100});
    REQUIRE(child->geometries == 1);
    (void)first.prepare({0, 0});
    child->geometry_changed();
    first.remove(child);
    const auto before = child->geometries;
    (void)first.prepare({200, 100});
    REQUIRE(child->geometries == before);
    centui::document second{child};
    (void)second.prepare({200, 100});
    REQUIRE(child->geometries == before + 1);
    auto weak = child.weak();
    child->geometry_changed();
    second.remove(child);
    child = {};
    REQUIRE_FALSE(weak.lock().valid());
}

TEST_CASE("Interactive changes refresh inherited appearance without an input transition", "[invalidation][cache]")
{
    auto child = centui::make_widget<probe>();
    child->patch_style({.width = centui::px(20), .height = centui::px(20)});
    child->set_paint_boundary(true);
    auto parent = centui::make_widget<probe>();
    parent->interactive(true);
    parent->append(child);
    parent->patch_style({.width = centui::px(100), .height = centui::px(100)});
    centui::document document{parent};
    document.process_event({centui::input_event_type::pointer_moved, 80, 80}, {200, 100});
    REQUIRE(parent->hovered());
    (void)document.prepare({200, 100});
    const auto before = document.statistics();
    child->interactive(true);
    REQUIRE(parent->hovered());
    REQUIRE_FALSE(child->hovered());
    (void)document.prepare({200, 100});
    REQUIRE(child->paints == 2);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    // Explicit state colors make the dependency visible in semantic commands.
    auto appearance = child->style();
    appearance.text_color.normal = {255, 0, 0, 255};
    appearance.text_color.hover = {0, 255, 0, 255};
    child->set_style(appearance);
    const auto local = document.prepare({200, 100});
    child->interactive(false);
    const auto changed = document.prepare({200, 100});
    REQUIRE(local.commands.back().color != changed.commands.back().color);
}

TEST_CASE("Hover focus capture and disable preserve an unrelated boundary", "[invalidation][cache]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    a->interactive(true);
    a->focusable(true);
    b->set_paint_boundary(true);
    centui::document document{centui::container{a, b}};
    (void)document.prepare({200, 100});
    const auto before = document.statistics();
    document.process_event({centui::input_event_type::pointer_moved, 5, 5}, {200, 100});
    (void)document.prepare({200, 100});
    REQUIRE(a->hovered());
    REQUIRE(a->request_focus());
    (void)document.prepare({200, 100});
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    (void)document.prepare({200, 100});
    REQUIRE(a->pressed());
    REQUIRE_FALSE(a->focus_visible());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {200, 100});
    REQUIRE(a->focus_visible());
    (void)document.prepare({200, 100});
    REQUIRE(b->paints == 1);
    a->set_enabled(false);
    (void)document.prepare({200, 100});
    REQUIRE_FALSE(a->focused());
    REQUIRE_FALSE(a->pressed());
    REQUIRE(b->paints == 1);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
}

TEST_CASE("Scale-only changes refresh commands without layout or geometry callbacks", "[invalidation][dpi]")
{
    auto child = centui::make_widget<probe>();
    centui::document document{child};
    (void)document.prepare({200, 100});
    const auto bounds = child->bounds();
    const auto before = document.statistics();
    const auto& drawing = document.prepare({400, 200, 2});
    REQUIRE(drawing.viewport == (centui::viewport{400, 200, 2}));
    REQUIRE(child->bounds() == bounds);
    REQUIRE(document.statistics().layout_nodes_synchronized == before.layout_nodes_synchronized);
    REQUIRE(document.statistics().measurement_calls == before.measurement_calls);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    REQUIRE(document.statistics().geometry_callbacks == before.geometry_callbacks);
    (void)document.prepare({600, 200, 2});
    REQUIRE(document.statistics().layout_nodes_synchronized == before.layout_nodes_synchronized);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations + 1);
}

TEST_CASE("Input caret selection and wheel scrolling refresh host bounds without Yoga", "[invalidation][input]")
{
    auto control = centui::input{"one two three four five six seven eight nine ten"};
    control->patch_style({.width = centui::px(80), .height = centui::px(30)});
    std::optional<centui::rect> host_area;
    centui::document document{control};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    auto services = std::make_shared<centui::host_services>();
    services->text_input = [&](std::optional<centui::rect> area)
    {
        host_area = area;
    };
    document.set_host_services(services);
    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    control->set_selection(0, 0);
    (void)document.prepare({200, 100});
    const auto initial = host_area;
    REQUIRE(initial.has_value());
    const auto before = document.statistics();
    control->set_selection(2, 2);
    (void)document.prepare({200, 100});
    REQUIRE(host_area != initial);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    REQUIRE(document.statistics().measurement_calls == before.measurement_calls);
    REQUIRE(document.statistics().geometry_callbacks == before.geometry_callbacks + 1);
    const auto selected = host_area;
    document.process_event({.type = centui::input_event_type::wheel, .x = 5, .y = 5, .delta_x = 20}, {200, 100});
    REQUIRE(host_area != selected);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);

    document.process_event({.type = centui::input_event_type::composition, .text = "replacement"}, {200, 100});
    auto calculations = document.statistics().layout_calculations;
    control->set_selection(0, 0);
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().layout_calculations == calculations + 1);
    document.process_event({.type = centui::input_event_type::composition, .text = "replacement"}, {200, 100});
    calculations = document.statistics().layout_calculations;
    document.process_event({.type = centui::input_event_type::pointer_pressed, .x = 5, .y = 5}, {200, 100});
    REQUIRE(document.statistics().layout_calculations == calculations + 1);
}

TEST_CASE("Fixed input and scroll bounds refresh geometry when their contents change", "[invalidation][geometry]")
{
    auto input = centui::input{"short"};
    input->patch_style({.width = centui::px(80), .height = centui::px(30)});
    auto content = centui::container{};
    content->patch_style({.height = centui::px(100), .flex = centui::flex::all(0, 0, centui::automatic())});
    auto scroll = centui::scroll{content};
    scroll->patch_style({.width = centui::px(80), .height = centui::px(30)});
    centui::document document{centui::container{input, scroll}};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    (void)document.prepare({200, 100});
    const auto input_bounds = input->bounds();
    const auto scroll_bounds = scroll->bounds();
    const auto limit = scroll->limit().y;
    content->patch_style({.height = centui::px(200)});
    input->set_text("A longer replacement string");
    const auto actual = document.prepare({200, 100});
    REQUIRE(input->bounds() == input_bounds);
    REQUIRE(scroll->bounds() == scroll_bounds);
    REQUIRE(scroll->limit().y > limit);
    auto fresh_input = centui::input{"A longer replacement string"};
    fresh_input->set_style(input->style());
    auto fresh_content = centui::container{};
    fresh_content->set_style(content->style());
    auto fresh_scroll = centui::scroll{fresh_content};
    fresh_scroll->set_style(scroll->style());
    centui::document reference{centui::container{fresh_input, fresh_scroll}};
    reference.set_font(centui::font{CENTUI_FONT_PATH});
    same_drawing(actual, reference.prepare({200, 100}));
    REQUIRE(scroll->limit() == fresh_scroll->limit());
}

TEST_CASE("Inherited metric changes synchronize dependents and match fresh text output", "[invalidation][font]")
{
    auto text = centui::make_widget<centui::text_widget>("Inherited font", centui::style_source{}, true);
    auto explicit_text = centui::text{"Explicit font"};
    const centui::font original{CENTUI_FONT_PATH};
    const centui::font replacement{CENTUI_FONT_PATH};
    explicit_text->patch_style({.font = original});
    auto root = centui::container{text, explicit_text};
    root->patch_style({.font = original});
    centui::document document{root};
    document.set_font(original);
    (void)document.prepare({300, 100});
    const auto before = document.statistics();
    root->patch_style({.font_size = 24, .font = replacement});
    const auto actual = document.prepare({300, 100});
    REQUIRE(document.statistics().layout_nodes_synchronized >= before.layout_nodes_synchronized + 3);
    REQUIRE(document.statistics().measurement_calls > before.measurement_calls);
    auto fresh_text = centui::make_widget<centui::text_widget>("Inherited font", centui::style_source{}, true);
    auto fresh_explicit = centui::text{"Explicit font"};
    fresh_explicit->set_style(explicit_text->style());
    auto fresh_root = centui::container{fresh_text, fresh_explicit};
    fresh_root->set_style(root->style());
    centui::document reference{fresh_root};
    reference.set_font(original);
    same_drawing(actual, reference.prepare({300, 100}));
    REQUIRE(text->bounds() == fresh_text->bounds());
    REQUIRE(explicit_text->bounds() == fresh_explicit->bounds());
    document.set_font(replacement);
    const auto calculations = document.statistics().layout_calculations;
    (void)document.prepare({300, 100});
    REQUIRE(document.statistics().layout_calculations == calculations + 1);
}

TEST_CASE("Popup focus settles on opening and reopening", "[invalidation][geometry][popup]")
{
    auto previous = centui::input{"Previous focus"};
    auto content = centui::input{"Popup input"};
    auto popup = centui::popup{centui::container{content}};
    centui::document document{centui::container{previous, popup}};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    (void)document.prepare({200, 100});
    REQUIRE(previous->request_focus());
    for(int opening = 0; opening != 2; ++opening)
    {
        popup->open();
        (void)document.prepare({200, 100});
        REQUIRE(content->focused());
        REQUIRE_FALSE(popup->focused());
        REQUIRE(content->bounds().height > 0);
        REQUIRE_FALSE(document.pending_changes().any());
        popup->close();
        (void)document.prepare({200, 100});
        REQUIRE_FALSE(content->focused());
        REQUIRE(previous->focused());
    }
}

TEST_CASE("Popup focus falls back to the popup when content is ineligible", "[invalidation][geometry][popup]")
{
    auto content = centui::input{"Disabled input"};
    content->set_enabled(false);
    auto popup = centui::popup{centui::container{content}};
    centui::document document{centui::container{popup}};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    (void)document.prepare({200, 100});
    for(int opening = 0; opening != 2; ++opening)
    {
        popup->open();
        (void)document.prepare({200, 100});
        REQUIRE(popup->focused());
        REQUIRE_FALSE(content->focused());
        REQUIRE_FALSE(document.pending_changes().any());
        popup->close();
        (void)document.prepare({200, 100});
        REQUIRE_FALSE(popup->focused());
    }
}

TEST_CASE(
    "Scale changes preserve retained physical pointer adjustment and logical event coordinates",
    "[invalidation][dpi]"
)
{
    auto child = centui::make_widget<probe>();
    child->interactive(true);
    child->patch_style({.width = centui::px(20), .height = centui::px(20)});
    centui::document document{child};
    document.process_event({centui::input_event_type::pointer_moved, 30, 5}, {200, 100});
    REQUIRE_FALSE(child->hovered());
    const auto options = document.options();
    const auto before = document.statistics();
    (void)document.prepare({400, 200, 2});
    REQUIRE(child->hovered());
    REQUIRE(document.options() == options);
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    document.process_event({centui::input_event_type::pointer_moved, 30, 5}, {400, 200, 2});
    REQUIRE_FALSE(child->hovered());
    auto changed = options;
    changed.point_scale_factor = 2;
    document.set_layout_options(changed);
    (void)document.prepare({400, 200, 2});
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations + 1);
}

TEST_CASE("Child clipping refreshes targeting and paint without intrinsic measurement", "[invalidation][cache]")
{
    auto child = centui::make_widget<probe>();
    child->interactive(true);
    child->patch_style({.width = centui::px(60), .height = centui::px(20)});
    auto parent = centui::make_widget<probe>();
    parent->patch_style({.width = centui::px(20), .height = centui::px(20)});
    parent->append(child);
    centui::document document{parent};
    document.process_event({centui::input_event_type::pointer_moved, 40, 5}, {200, 100});
    REQUIRE(child->hovered());
    (void)document.prepare({200, 100});
    const auto before = document.statistics();
    parent->clipping(true);
    (void)document.prepare({200, 100});
    REQUIRE_FALSE(child->hovered());
    REQUIRE(document.statistics().layout_calculations == before.layout_calculations);
    REQUIRE(document.statistics().measurement_calls == before.measurement_calls);
}

TEST_CASE("Leaf transitions and zero viewport retain pending preparation", "[invalidation][layout]")
{
    auto root = centui::make_widget<probe>();
    // A maximum width requires intrinsic sizing; a viewport-sized root can skip measurement.
    root->patch_style({.align_items = centui::alignment::start, .max_width = centui::px(200)});
    centui::document document{root};
    (void)document.prepare({200, 100});
    auto child = centui::make_widget<probe>();
    document.append(root, child);
    (void)document.prepare({200, 100});
    (void)document.prepare({0, 0});
    document.remove(child);
    root->natural_width = 60;
    root->metrics_changed();
    root->geometry_changed();
    REQUIRE(document.pending_changes().layout);
    REQUIRE(document.pending_changes().paint);
    const auto before = document.statistics();
    (void)document.prepare({0, 0});
    REQUIRE(document.statistics().geometry_callbacks == before.geometry_callbacks);
    REQUIRE(document.pending_changes().paint);
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().measurement_calls > before.measurement_calls);
    REQUIRE(root->bounds().width == 60);
    REQUIRE_FALSE(document.pending_changes().any());
    auto fresh = centui::make_widget<probe>();
    fresh->natural_width = 60;
    fresh->set_style(root->style());
    centui::document reference{fresh};
    same_drawing(document.prepare({200, 100}), reference.prepare({200, 100}));
    REQUIRE(root->bounds() == fresh->bounds());
}

TEST_CASE("Measurement and baseline failures retry after native inputs were synchronized", "[invalidation][layout]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    auto root = centui::container{a, b};
    root->patch_style({.flex_direction = centui::direction::row, .align_items = centui::alignment::baseline});
    centui::document document{root};
    (void)document.prepare({200, 100});
    SECTION("measurement")
    {
        a->fail_measure = true;
    }
    SECTION("baseline")
    {
        a->fail_baseline = true;
    }
    a->metrics_changed();
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::runtime_error);
    REQUIRE(document.pending_changes().layout);
    a->fail_measure = a->fail_baseline = false;
    (void)document.prepare({200, 100});
    REQUIRE(a->bounds().width == 20);
    REQUIRE_FALSE(document.pending_changes().any());
}

TEST_CASE("Failed paint retains published drawing and retries nested boundaries", "[invalidation][cache]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    a->set_paint_boundary(true);
    auto root = centui::container{a, b};
    root->set_paint_boundary(true);
    centui::document document{root};
    const auto& published = document.prepare({200, 100});
    const auto previous = published;
    b->fail_paint = true;
    b->drawing_changed();
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::runtime_error);
    same_drawing(published, previous);
    REQUIRE(document.pending_changes().paint);
    b->fail_paint = false;
    same_drawing(document.prepare({200, 100}), previous);
    document.set_command_cache_budget(0);
    b->drawing_changed();
    same_drawing(document.prepare({200, 100}), previous);
}

TEST_CASE("Incremental Yoga dependencies match a fresh final tree", "[invalidation][layout]")
{
    // Each mode exercises the same mutation sequence against a fresh reference.
    for(int mode = 0; mode != 5; ++mode)
    {
        auto a = centui::make_widget<probe>();
        auto b = centui::make_widget<probe>();
        auto branch = centui::container{a};
        auto root = centui::container{branch, b};
        centui::style appearance;
        appearance.width = centui::px(100);
        appearance.height = centui::px(80);
        appearance.flex_direction = centui::direction::row;
        appearance.align_items = mode == 3 ? centui::alignment::baseline : centui::alignment::start;
        if(mode == 0)
        {
            appearance.flex_wrap = centui::wrapping::wrap;
        }
        root->set_style(appearance);
        if(mode == 1)
        {
            b->patch_style({.width = centui::percent(60)});
        }
        if(mode == 2)
        {
            b->patch_style({.flex = centui::flex::all(1, 0, centui::px(0))});
        }
        if(mode == 4)
        {
            branch->patch_style({.display = centui::display::contents});
        }
        centui::document document{root};
        (void)document.prepare({200, 100});
        a->natural_width = 90;
        a->metrics_changed();
        const auto actual = document.prepare({200, 100});

        auto fresh_a = centui::make_widget<probe>();
        auto fresh_b = centui::make_widget<probe>();
        fresh_a->natural_width = 90;
        fresh_b->set_style(b->style());
        auto fresh_branch = centui::container{fresh_a};
        fresh_branch->set_style(branch->style());
        auto fresh_root = centui::container{fresh_branch, fresh_b};
        fresh_root->set_style(appearance);
        centui::document reference{fresh_root};
        const auto expected = reference.prepare({200, 100});
        REQUIRE(root->bounds() == fresh_root->bounds());
        REQUIRE(branch->bounds() == fresh_branch->bounds());
        REQUIRE(a->bounds() == fresh_a->bounds());
        REQUIRE(b->bounds() == fresh_b->bounds());
        same_drawing(actual, expected);
    }
}
