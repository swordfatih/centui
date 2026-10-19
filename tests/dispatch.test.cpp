#include <array>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import centui.document;
import centui.element;
import centui.model;
import centui.runtime;
import centui.container;
import centui.text;
import centui.button;
import centui.ui;
import centui.input;
import centui.glyph;
import centui.draw;
import centui.unit;
import centui.color;

namespace dispatch_tests
{

struct backend
{
    centui::font                      face;
    centui::viewport                  size{400, 240};
    std::vector<centui::draw_command> commands;

    centui::viewport viewport() const
    {
        return size;
    }

    centui::font& glyphs()
    {
        return face;
    }

    void draw(std::span<const centui::draw_command> frame)
    {
        commands.assign(frame.begin(), frame.end());
    }
};

void fixed(centui::element& value)
{
    value.patch_style({.width = centui::px(100), .height = centui::px(40)});
}

std::array<centui::input_event, 2> click(float x = 10, float y = 10)
{
    return {
        centui::input_event{.type = centui::input_event_type::pointer_pressed, .x = x, .y = y},
        centui::input_event{.type = centui::input_event_type::pointer_released, .x = x, .y = y},
    };
}

auto state(const centui::element& value)
{
    return centui::details::element_access::state(value);
}

// A generic box with activation, deliberately unrelated to the button widget.
class surface : public centui::element
{
public:
    explicit surface(std::function<void()> callback = {})
        : element(make_content(std::move(callback)))
    {
    }

    void set_enabled(bool enabled)
    {
        set_enabled_state(enabled);
    }

    void set_children(std::initializer_list<centui::element> children)
    {
        compose(std::span<const centui::element>{children.begin(), children.size()});
    }

private:
    static centui::details::element_content make_content(std::function<void()> callback)
    {
        centui::details::interaction_capability capability;
        if(callback)
        {
            capability.activate = std::make_shared<std::function<void()>>(std::move(callback));
        }
        return {.data = centui::container_data{}, .style = {}, .interaction = std::move(capability)};
    }
};

} // namespace dispatch_tests

TEST_CASE("Generic interaction capabilities work across UI adapter instances", "[input][primitives]")
{
    dispatch_tests::backend backend;
    int                     clicks = 0;
    dispatch_tests::surface target{[&]
    {
        ++clicks;
    }};
    dispatch_tests::fixed(target);
    centui::document document{target};
    const auto       events = dispatch_tests::click();
    {
        centui::ui ui{backend};
        ui.process_event(document, events[0]);
    }
    REQUIRE(dispatch_tests::state(target)->interaction.captured == target.id());
    centui::ui another_ui{backend};
    another_ui.process_event(document, events[1]);
    REQUIRE(clicks == 1);
    REQUIRE_FALSE(dispatch_tests::state(target)->interaction.captured);
}

TEST_CASE("Button caption is private and inherits typography and interaction colors", "[primitives][button]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::button          action{"Caption"};
    action.patch_style({.padding = centui::edges::all(centui::px(5)), .width = centui::px(160), .height = centui::px(60), .font_size = 24});
    const auto normal = centui::color::from_hex(0xFF0000);
    const auto hovered = centui::color::from_hex(0x00FF00);
    const auto disabled = centui::color::from_hex(0x999999);
    action.patch_style({.text_color = centui::states_color{normal, hovered, normal, normal, disabled}});
    centui::document document{action};
    document.enable_preparation_diagnostics();
    REQUIRE(document.children(action).empty());
    REQUIRE_THROWS_AS(document.append(action, centui::text{"Foreign child"}), std::invalid_argument);
    ui.render(document);
    const auto& initial = std::get<centui::text_command>(backend.commands.back());
    REQUIRE(initial.value == "Caption");
    REQUIRE(initial.font_size == 24);
    REQUIRE(initial.color == normal);
    REQUIRE(initial.bounds.x == 5);
    REQUIRE(initial.bounds.y == 5);
    const auto before = document.preparation_stats();

    ui.process_event(document, {.type = centui::input_event_type::pointer_moved, .x = 10, .y = 10});
    ui.render(document);
    REQUIRE(std::get<centui::text_command>(backend.commands.back()).color == hovered);
    REQUIRE(document.preparation_stats().shapings == before.shapings);
    REQUIRE(document.preparation_stats().layout_passes == before.layout_passes);
    action.set_enabled(false);
    ui.render(document);
    REQUIRE(std::get<centui::text_command>(backend.commands.back()).color == disabled);

    action.set_text("Updated caption");
    action.patch_style({.font_size = 30});
    ui.render(document);
    REQUIRE(action.value() == "Updated caption");
    REQUIRE(std::get<centui::text_command>(backend.commands.back()).value == action.value());
    REQUIRE(std::get<centui::text_command>(backend.commands.back()).font_size == 30);
    REQUIRE(document.preparation_stats().shapings == before.shapings + 1);
}

TEST_CASE("Later events see geometry changed by the previous activation", "[input][geometry]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::button          first{"First"};
    int                     clicks = 0;
    centui::button          second{"Second", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(first);
    dispatch_tests::fixed(second);
    centui::container root{first, second};
    centui::document  document{root};
    first.set_on_click([&]
    {
        auto margin = centui::edges::all(centui::px(0));
        margin.top = centui::px(100);
        second.patch_style({.margin = margin});
    });
    const auto       first_click = dispatch_tests::click();
    const auto       second_click = dispatch_tests::click(10, 150);
    const std::array events{first_click[0], first_click[1], second_click[0], second_click[1]};
    ui.process_events(document, events);
    REQUIRE(clicks == 1);
    REQUIRE(dispatch_tests::state(second)->absolute_bounds(second.id()).y == 140);
}

TEST_CASE("Removal and insertion during activation cannot revive an old target", "[input][lifetime]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::button          first{"First"};
    int                     removed_clicks = 0;
    int                     replacement_clicks = 0;
    centui::button          removed{"Removed", {.on_click = [&]
             {
        ++removed_clicks;
    }}};
    centui::button          replacement{"Replacement", {.on_click = [&]
             {
        ++replacement_clicks;
    }}};
    dispatch_tests::fixed(first);
    dispatch_tests::fixed(removed);
    dispatch_tests::fixed(replacement);
    centui::container root{first, removed};
    centui::document  document{root};
    first.set_on_click([&]
    {
        document.remove(removed);
        document.append(root, replacement);
    });
    const auto       a = dispatch_tests::click();
    const auto       b = dispatch_tests::click(10, 50);
    const std::array events{a[0], a[1], b[0], b[1]};
    ui.process_events(document, events);
    REQUIRE_FALSE(removed.valid());
    REQUIRE(removed_clicks == 0);
    REQUIRE(replacement_clicks == 1);
}

TEST_CASE("Self removal stops later activations in the same batch", "[input][lifetime]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::button          action{"Remove"};
    dispatch_tests::fixed(action);
    centui::document document{action};
    int              clicks = 0;
    action.set_on_click([&]
    {
        ++clicks;
        document.remove(action);
    });
    const auto       click = dispatch_tests::click();
    const std::array events{click[0], click[1], click[0], click[1]};
    ui.process_events(document, events);
    REQUIRE(clicks == 1);
    REQUIRE(document.empty());
    REQUIRE_FALSE(action.valid());
}

TEST_CASE("Root replacement is visible to the next event in the batch", "[input][lifetime]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::button          original{"Original"};
    int                     clicks = 0;
    centui::button          replacement{"Replacement", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(original);
    dispatch_tests::fixed(replacement);
    centui::document document{original};
    original.set_on_click([&]
    {
        document.remove(original);
        document.mount(replacement);
    });
    const auto       click = dispatch_tests::click();
    const std::array events{click[0], click[1], click[0], click[1]};
    ui.process_events(document, events);
    REQUIRE_FALSE(original.valid());
    REQUIRE(document.root() == replacement);
    REQUIRE(clicks == 1);
}

TEST_CASE("Disabling cancels targets immediately and reenabling does not restore capture", "[input]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(action);
    centui::document document{action};
    const auto       events = dispatch_tests::click();
    ui.process_event(document, events[0]);
    auto state = dispatch_tests::state(action);
    REQUIRE(state->interaction.captured == action.id());
    action.set_enabled(false);
    REQUIRE_FALSE(state->interaction.captured);
    REQUIRE_FALSE(state->interaction.focused);
    REQUIRE_FALSE(state->interaction.hovered);
    action.set_enabled(true);
    ui.process_event(document, events[1]);
    REQUIRE(clicks == 0);
}

TEST_CASE("Pointer exit preserves capture while release outside cancels activation", "[input]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(action);
    centui::document document{action};
    const auto       events = dispatch_tests::click();
    ui.process_event(document, events[0]);
    ui.process_event(document, {.type = centui::input_event_type::pointer_left});
    auto state = dispatch_tests::state(action);
    REQUIRE_FALSE(state->interaction.hovered);
    REQUIRE(state->interaction.captured == action.id());
    ui.process_event(document, events[1]);
    REQUIRE(clicks == 1);

    ui.process_event(document, events[0]);
    ui.process_event(document, {.type = centui::input_event_type::pointer_released, .x = 300, .y = 200});
    REQUIRE(clicks == 1);
    REQUIRE_FALSE(state->interaction.captured);
}

TEST_CASE("Focus loss and cancellation clear hover focus and capture", "[input]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(action);
    centui::document document{action};
    const auto       events = dispatch_tests::click();
    ui.process_event(document, events[0]);
    ui.process_event(document, {.type = centui::input_event_type::focus_lost});
    auto state = dispatch_tests::state(action);
    REQUIRE_FALSE(state->interaction.hovered);
    REQUIRE_FALSE(state->interaction.focused);
    REQUIRE_FALSE(state->interaction.captured);
    ui.process_event(document, events[1]);
    REQUIRE(clicks == 0);
    ui.process_event(document, {.type = centui::input_event_type::focus_gained});
    ui.process_events(document, events);
    REQUIRE(clicks == 1);
    ui.process_event(document, events[0]);
    ui.process_event(document, {.type = centui::input_event_type::pointer_cancelled});
    REQUIRE_FALSE(state->interaction.hovered);
    REQUIRE_FALSE(state->interaction.focused);
    REQUIRE_FALSE(state->interaction.captured);
    ui.process_event(document, events[1]);
    REQUIRE(clicks == 1);
}

TEST_CASE("Zero viewport cancels capture even with no queued events", "[input][viewport]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(action);
    centui::document document{action};
    const auto       events = dispatch_tests::click();
    ui.process_event(document, events[0]);
    backend.size = {};
    ui.process_events(document, {});
    REQUIRE_FALSE(dispatch_tests::state(action)->interaction.captured);
    backend.size = {400, 240};
    ui.process_event(document, events[1]);
    REQUIRE(clicks == 0);
    ui.process_events(document, events);
    REQUIRE(clicks == 1);
}

TEST_CASE("Input sees a resized viewport before any render call", "[input][viewport]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
    }}};
    action.patch_style({.width = centui::percent(50), .height = centui::px(40)});
    centui::document document{action};
    document.enable_preparation_diagnostics();
    ui.render(document);
    const auto before = document.preparation_stats();
    backend.size.width = 800;
    ui.process_events(document, dispatch_tests::click(300, 10));
    REQUIRE(clicks == 1);
    REQUIRE(document.preparation_stats().layout_passes == before.layout_passes + 1);
    REQUIRE(document.preparation_stats().shapings == before.shapings);
}

TEST_CASE("Viewport changes inside callbacks affect subsequent events", "[input][viewport]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
        backend.size.width = 800;
    }}};
    action.patch_style({.width = centui::percent(50), .height = centui::px(40)});
    centui::document document{action};
    const auto       a = dispatch_tests::click();
    const auto       b = dispatch_tests::click(300, 10);
    const std::array events{a[0], a[1], b[0], b[1]};
    ui.process_events(document, events);
    REQUIRE(clicks == 2);
}

TEST_CASE("Overlapping interaction surfaces follow reverse paint order", "[input][paint]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     first_clicks = 0;
    int                     last_clicks = 0;
    centui::button          first{"First", {.on_click = [&]
             {
        ++first_clicks;
    }}};
    centui::button          last{"Last", {.on_click = [&]
             {
        ++last_clicks;
    }}};
    for(auto* action: {&first, &last})
    {
        dispatch_tests::fixed(*action);
        action->patch_style({.inset = centui::edges::all(centui::px(0)), .position = centui::position::absolute});
    }
    centui::container root{first, last};
    centui::document  document{root};
    ui.render(document);
    REQUIRE(std::get<centui::text_command>(backend.commands.back()).value == "Last");
    ui.process_events(document, dispatch_tests::click());
    REQUIRE(first_clicks == 0);
    REQUIRE(last_clicks == 1);
    const std::array<centui::element, 2> order{last, first};
    document.reorder(root, order);
    ui.process_events(document, dispatch_tests::click());
    REQUIRE(first_clicks == 1);
    REQUIRE(last_clicks == 1);
}

TEST_CASE("Callback exceptions preserve mutations and release the dispatch guard", "[input][exceptions]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action"};
    dispatch_tests::fixed(action);
    centui::document document{action};
    action.set_on_click([&]
    {
        action.set_text(std::to_string(++clicks));
        if(clicks == 1)
        {
            throw std::runtime_error("Application callback failed");
        }
    });
    const auto       click = dispatch_tests::click();
    const std::array events{click[0], click[1], click[0], click[1]};
    REQUIRE_THROWS_AS(ui.process_events(document, events), std::runtime_error);
    REQUIRE(clicks == 1);
    REQUIRE(action.value() == "1");
    auto state = dispatch_tests::state(action);
    REQUIRE_FALSE(state->interaction.dispatching);
    REQUIRE_FALSE(state->interaction.captured);
    ui.process_events(document, click);
    REQUIRE(clicks == 2);
    REQUIRE(action.value() == "2");
}

TEST_CASE("Recursive dispatch into the same runtime is rejected and recoverable", "[input][exceptions]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::ui              another_ui{backend};
    centui::button          action{"Action"};
    dispatch_tests::fixed(action);
    centui::document document{action};
    action.set_on_click([&]
    {
        another_ui.process_events(document, dispatch_tests::click());
    });
    REQUIRE_THROWS_AS(ui.process_events(document, dispatch_tests::click()), std::logic_error);
    REQUIRE_FALSE(dispatch_tests::state(action)->interaction.dispatching);
    int clicks = 0;
    action.set_on_click([&]
    {
        ++clicks;
    });
    ui.process_events(document, dispatch_tests::click());
    REQUIRE(clicks == 1);
}

TEST_CASE("Document replacement retires old handles while its callback is still executing", "[input][lifetime]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    centui::button          first{"First"};
    int                     replacement_clicks = 0;
    centui::button          second{"Second", {.on_click = [&]
             {
        ++replacement_clicks;
    }}};
    dispatch_tests::fixed(first);
    dispatch_tests::fixed(second);
    centui::document document{first};
    centui::document replacement{second};
    bool             retired_immediately = false;
    first.set_on_click([&]
    {
        document = std::move(replacement);
        retired_immediately = !first.valid();
    });
    const auto       click = dispatch_tests::click();
    const std::array events{click[0], click[1], click[0], click[1]};
    ui.process_events(document, events);
    REQUIRE(retired_immediately);
    REQUIRE(replacement_clicks == 1);
    REQUIRE(document.root() == second);
}

TEST_CASE("Reparenting into a disabled surface cancels descendant capture immediately", "[input][topology]")
{
    dispatch_tests::backend backend;
    centui::ui              ui{backend};
    int                     clicks = 0;
    centui::button          action{"Action", {.on_click = [&]
             {
        ++clicks;
    }}};
    dispatch_tests::fixed(action);
    centui::container       left{action};
    dispatch_tests::surface disabled;
    disabled.set_enabled(false);
    centui::container root{left, disabled};
    centui::document  document{root};
    const auto        events = dispatch_tests::click();
    ui.process_event(document, events[0]);
    auto state = dispatch_tests::state(action);
    REQUIRE(state->interaction.captured == action.id());
    document.reparent(action, disabled);
    REQUIRE_FALSE(state->interaction.captured);
    REQUIRE_FALSE(state->interaction.focused);
    REQUIRE_FALSE(state->interaction.hovered);
    ui.process_event(document, events[1]);
    REQUIRE(clicks == 0);
}

TEST_CASE("Subtree dispatch uses document coordinates and clears capture outside its scope", "[input][subtree]")
{
    dispatch_tests::backend backend;
    centui::ui ui{backend};
    int first_clicks = 0;
    int second_clicks = 0;
    centui::button first{"First", {.on_click = [&]
    {
        ++first_clicks;
    }}};
    centui::button second{"Second", {.on_click = [&]
    {
        ++second_clicks;
    }}};
    dispatch_tests::fixed(first);
    dispatch_tests::fixed(second);
    centui::container branch{second};
    branch.patch_style({.padding = centui::edges::all(centui::px(5))});
    centui::container root{first, branch};
    root.patch_style({.padding = centui::edges::all(centui::px(20))});
    centui::document document{root};

    ui.process_events(branch, dispatch_tests::click(30, 70));
    REQUIRE(second_clicks == 1);
    const auto events = dispatch_tests::click(25, 25);
    ui.process_event(document, events[0]);
    auto state = dispatch_tests::state(first);
    REQUIRE(state->interaction.captured == first.id());
    ui.process_event(branch, events[1]);
    REQUIRE_FALSE(state->interaction.captured);
    REQUIRE(first_clicks == 0);
}

TEST_CASE("A callback can destroy the construction handle used to dispatch its batch", "[input][lifetime]")
{
    dispatch_tests::backend backend;
    centui::ui ui{backend};
    std::optional<centui::button<>> action{std::in_place, "Action"};
    dispatch_tests::fixed(*action);
    auto runtime = dispatch_tests::state(*action);
    const auto id = action->id();
    int clicks = 0;
    action->set_on_click([&]
    {
        ++clicks;
        action.reset();
    });
    const auto click = dispatch_tests::click();
    const std::array events{click[0], click[1], click[0], click[1]};

    ui.process_events(*action, events);
    REQUIRE(clicks == 1);
    REQUIRE_FALSE(action);
    REQUIRE_FALSE(runtime->contains(id));
    REQUIRE_FALSE(runtime->interaction.dispatching);
}
