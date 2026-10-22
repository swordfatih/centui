#include <array>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import centui;

namespace
{
struct panel_style
{
    centui::length width = centui::px(200);
    centui::length height = centui::px(100);
};

struct button_style
{
    centui::length width = centui::px(80);
    centui::length height = centui::px(30);
};

class counted_widget final : public centui::widget
{
public:
    explicit counted_widget(int& paints) : _paints(paints)
    {
        patch_style({.width = centui::px(20), .height = centui::px(20)});
    }

protected:
    void paint(centui::painter& output) const override
    {
        ++_paints;
        output.rectangle(output.bounds(), {255, 0, 0, 255});
    }

private:
    int& _paints;
};

class guarded_measurement final : public centui::widget
{
public:
    bool mutate = true;

protected:
    centui::size measure(const centui::measure_context&) override
    {
        if(mutate)
        {
            patch_style({.width = centui::px(10)});
        }
        return {10, 10};
    }
};

class pointer_probe final : public centui::widget
{
public:
    pointer_probe()
    {
        patch_style({.width = centui::px(80), .height = centui::px(30)});
        set_interactive();
    }

    std::vector<centui::input_event> events;
    std::function<void()> on_cancel;

protected:
    bool on_event(const centui::input_event& event) override
    {
        events.push_back(event);
        if(event.type == centui::input_event_type::pointer_cancelled && on_cancel)
        {
            on_cancel();
        }
        return true;
    }
};

void click(centui::document& document, float x = 5, float y = 5)
{
    const std::array events{
        centui::input_event{centui::input_event_type::pointer_pressed, x, y, centui::mouse_button::primary},
        centui::input_event{centui::input_event_type::pointer_released, x, y, centui::mouse_button::primary}};
    document.process_events(events, {200, 100});
}
}

TEST_CASE("Document removal and destruction detach surviving handles", "[document]")
{
    auto child = centui::container<>();
    {
        auto root = centui::container<panel_style>{child};
        centui::document document{root};
        REQUIRE(document.children(root).size() == 1);
        document.remove(child);
        REQUIRE(document.children(root).empty());
        REQUIRE(child.valid());
        document.append(root, child);
        REQUIRE(document.parent(child) == centui::handle<>{root});
    }
    REQUIRE(child.valid());
    centui::document next{child};
    REQUIRE(next.root() == centui::handle<>{child});
}

TEST_CASE("Idle preparation reuses paint and equal patches do no work", "[document][dirty]")
{
    int paints = 0;
    auto child = centui::make_widget<counted_widget>(paints);
    centui::document document{child};
    const auto* first = document.prepare({200, 100}).commands.data();
    REQUIRE(paints == 1);
    REQUIRE(document.prepare({200, 100}).commands.data() == first);
    REQUIRE(paints == 1);
    child.patch_style({.width = centui::px(20)});
    (void)document.prepare({200, 100});
    REQUIRE(paints == 1);
    child.patch_style({.background_color = centui::states_color::all({1, 2, 3, 255})});
    (void)document.prepare({200, 100});
    REQUIRE(paints == 2);
    REQUIRE_THROWS(child.patch_style({.width = centui::px(-1)}));
    REQUIRE(child.style().width == centui::px(20));
}

TEST_CASE("Mutable callback state survives events and registration replacement", "[document][input]")
{
    int observed = 0;
    auto button = centui::button<button_style>{"Count", {.on_click = [count = 0, &observed]() mutable
    {
        observed = ++count;
    }}};
    centui::document document{button};
    click(document);
    click(document);
    REQUIRE(observed == 2);

    const auto weak = button.weak();
    button.set_on_click([weak, &observed]()
    {
        if(auto self = weak.lock(); self.valid())
        {
            self->set_on_click([&observed]() { observed = 10; });
        }
        observed = 3;
    });
    click(document);
    REQUIRE(observed == 3);
    click(document);
    REQUIRE(observed == 10);
}

TEST_CASE("Release outside, focus loss and disabled controls cancel activation", "[document][input]")
{
    int calls = 0;
    auto button = centui::button<button_style>{"Click", {.on_click = [&calls]() { ++calls; }}};
    centui::document document{button};
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 150, 90}, {200, 100});
    REQUIRE(calls == 0);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::focus_lost}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(calls == 0);
    document.process_event({centui::input_event_type::focus_gained}, {200, 100});
    button.set_enabled(false);
    click(document);
    REQUIRE(calls == 0);
    button.set_enabled(true);
    click(document);
    REQUIRE(calls == 1);
}

TEST_CASE("Callback topology edits become visible before the next event", "[document][input]")
{
    int calls = 0;
    auto button = centui::button<button_style>{"Remove"};
    auto root = centui::container<panel_style>{button};
    centui::document document{root};
    const auto weak = button.weak();
    button.set_on_click([&document, &calls, weak]()
    {
        ++calls;
        document.remove(weak.lock());
    });
    const std::array events{
        centui::input_event{centui::input_event_type::pointer_pressed, 5, 5},
        centui::input_event{centui::input_event_type::pointer_released, 5, 5},
        centui::input_event{centui::input_event_type::pointer_pressed, 5, 5},
        centui::input_event{centui::input_event_type::pointer_released, 5, 5}};
    document.process_events(events, {200, 100});
    REQUIRE(calls == 1);
    REQUIRE(document.children(root).empty());
    REQUIRE(button.valid());
}

TEST_CASE("Callback exceptions release dispatch guards and preserve edits", "[document][input]")
{
    auto button = centui::button<button_style>{"Throw"};
    auto root = centui::container<panel_style>{button};
    centui::document document{root};
    const auto weak = button.weak();
    button.set_on_click([&document, weak]()
    {
        document.remove(weak.lock());
        throw std::runtime_error("callback");
    });
    REQUIRE_THROWS_AS(click(document), std::runtime_error);
    REQUIRE(document.children(root).empty());
    REQUIRE_NOTHROW(click(document));
}

TEST_CASE("Root replacement reaches the next event in a batch", "[document][input]")
{
    int calls = 0;
    auto first = centui::button<button_style>{"First"};
    auto second = centui::button<button_style>{"Second", {.on_click = [&calls]() { ++calls; }}};
    centui::document document{first};
    first.set_on_click([&document, second]() { document.replace_root(second); });
    click(document);
    click(document);
    REQUIRE(calls == 1);
    REQUIRE(document.root() == centui::handle<>{second});
}

TEST_CASE("Preparation mutations are rejected and failed measurement is retryable", "[document][exceptions]")
{
    auto child = centui::make_widget<guarded_measurement>();
    auto root = centui::container<panel_style>{child};
    root.patch_style({.align_items = centui::alignment::start});
    centui::document document{root};
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::logic_error);
    REQUIRE(document.pending_changes().layout);
    child->mutate = false;
    REQUIRE_NOTHROW(document.prepare({200, 100}));
    REQUIRE(child->bounds().width == 10);
    REQUIRE(child->bounds().height == 10);
}

TEST_CASE("Recursive dispatch is rejected without poisoning later input", "[document][exceptions]")
{
    auto button = centui::button<button_style>{"Recursive"};
    centui::document document{button};
    button.set_on_click([&document]() { click(document); });
    REQUIRE_THROWS_AS(click(document), std::logic_error);
    int calls = 0;
    button.set_on_click([&calls]() { ++calls; });
    REQUIRE_NOTHROW(click(document));
    REQUIRE(calls == 1);
}

TEST_CASE("Captured widgets receive outside movement and release for each mouse button", "[document][capture]")
{
    for(const auto button: {centui::mouse_button::primary, centui::mouse_button::secondary, centui::mouse_button::middle})
    {
        auto probe = centui::make_widget<pointer_probe>();
        centui::document document{probe};
        document.process_event({centui::input_event_type::pointer_pressed, 5, 5, button}, {200, 100});
        document.process_event({centui::input_event_type::pointer_left}, {200, 100});
        document.process_event({centui::input_event_type::pointer_moved, -20, 150}, {200, 100});
        document.process_event({centui::input_event_type::pointer_released, -20, 150, button}, {200, 100});
        REQUIRE(probe->events.size() == 3);
        REQUIRE(probe->events[0].type == centui::input_event_type::pointer_pressed);
        REQUIRE(probe->events[1].type == centui::input_event_type::pointer_moved);
        REQUIRE(probe->events[2].type == centui::input_event_type::pointer_released);
        REQUIRE(probe->events[2].button == button);
        REQUIRE(probe->events[2].x == -20);
        REQUIRE(probe->events[2].y == 150);
        document.process_event({centui::input_event_type::pointer_moved, -30, 160}, {200, 100});
        REQUIRE(probe->events.size() == 3);
    }
}

TEST_CASE("External capture cancellation guards callbacks and settles their edits", "[document][capture]")
{
    auto probe = centui::make_widget<pointer_probe>();
    auto root = centui::container<panel_style>{probe};
    centui::document document{root};
    bool throw_callback = false;
    SECTION("Successful cancellation") {}
    SECTION("Throwing cancellation") { throw_callback = true; }
    probe->on_cancel = [&]
    {
        REQUIRE_THROWS_AS(click(document), std::logic_error);
        document.remove(probe);
        REQUIRE(document.children(root).size() == 1);
        if(throw_callback)
        {
            throw std::runtime_error("cancel");
        }
    };
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5,
                            centui::mouse_button::secondary}, {200, 100});
    if(throw_callback)
    {
        REQUIRE_THROWS_AS(probe->set_enabled(false), std::runtime_error);
    }
    else
    {
        REQUIRE_NOTHROW(probe->set_enabled(false));
    }
    REQUIRE(document.children(root).empty());
    REQUIRE(probe->events.size() == 2);
    REQUIRE(probe->events.back().type == centui::input_event_type::pointer_cancelled);
    REQUIRE(probe->events.back().button == centui::mouse_button::secondary);
    REQUIRE_NOTHROW(click(document));
    REQUIRE(probe->events.size() == 2);
}

TEST_CASE("Stationary pointer hover follows layout and topology changes", "[document][hover]")
{
    const centui::color normal{10, 20, 30, 255};
    const centui::color hovered{40, 50, 60, 255};
    auto colors = centui::states_color::all(normal);
    colors.hover = hovered;
    auto first = centui::make_widget<pointer_probe>();
    auto second = centui::make_widget<pointer_probe>();
    first.patch_style({.background_color = colors});
    second.patch_style({.background_color = colors});
    auto root = centui::container<panel_style>{first, second};
    centui::document document{root};
    const auto color_at = [&](std::size_t index)
    {
        return std::get<centui::rectangle_command>(document.prepare({200, 100}).commands.at(index)).color;
    };
    document.process_event({centui::input_event_type::pointer_moved, 5, 35}, {200, 100});
    REQUIRE(color_at(0) == normal);
    REQUIRE(color_at(1) == hovered);
    first.patch_style({.height = centui::px(50)});
    REQUIRE(color_at(0) == hovered);
    REQUIRE(color_at(1) == normal);
    document.remove(first);
    REQUIRE(color_at(0) == normal);
    second.patch_style({.height = centui::px(50)});
    REQUIRE(color_at(0) == hovered);
    second->set_enabled(false);
    REQUIRE(color_at(0) == normal);
    second->set_enabled(true);
    REQUIRE(color_at(0) == hovered);
    document.process_event({centui::input_event_type::pointer_left}, {200, 100});
    second.patch_style({.height = centui::px(60)});
    REQUIRE(color_at(0) == normal);
    document.process_event({centui::input_event_type::pointer_moved, 5, 5}, {200, 100});
    REQUIRE(color_at(0) == hovered);
    document.process_event({centui::input_event_type::focus_lost}, {200, 100});
    document.process_event({centui::input_event_type::focus_gained}, {200, 100});
    REQUIRE(color_at(0) == normal);
}

TEST_CASE("Another mouse button cannot steal or end an active capture", "[document][capture]")
{
    auto probe = centui::make_widget<pointer_probe>();
    centui::document document{probe};
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5, centui::mouse_button::secondary}, {200, 100});
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5, centui::mouse_button::primary}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5, centui::mouse_button::primary}, {200, 100});
    REQUIRE(probe->events.size() == 1);
    document.process_event({centui::input_event_type::pointer_moved, 150, 90}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 150, 90, centui::mouse_button::secondary}, {200, 100});
    REQUIRE(probe->events.size() == 3);
    REQUIRE(probe->events.back().type == centui::input_event_type::pointer_released);
    REQUIRE(probe->events.back().button == centui::mouse_button::secondary);
}

TEST_CASE("Focus loss cancels a secondary capture without synthesizing release", "[document][capture]")
{
    auto probe = centui::make_widget<pointer_probe>();
    centui::document document{probe};
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5, centui::mouse_button::secondary}, {200, 100});
    document.process_event({centui::input_event_type::focus_lost}, {200, 100});
    REQUIRE(probe->events.size() == 2);
    REQUIRE(probe->events.back().type == centui::input_event_type::pointer_cancelled);
    REQUIRE(probe->events.back().button == centui::mouse_button::secondary);
    document.process_event({centui::input_event_type::focus_gained}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5, centui::mouse_button::secondary}, {200, 100});
    REQUIRE(probe->events.size() == 2);
}

TEST_CASE("Buttons activate only for a primary gesture ending inside", "[document][button]")
{
    int calls = 0;
    auto button = centui::button<button_style>{"Click", {.on_click = [&calls]() { ++calls; }}};
    centui::document document{button};
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5, centui::mouse_button::secondary}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5, centui::mouse_button::secondary}, {200, 100});
    REQUIRE(calls == 0);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 100, 40}, {200, 100});
    REQUIRE(calls == 0);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 150, 90}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5, centui::mouse_button::secondary}, {200, 100});
    REQUIRE(calls == 0);
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(calls == 1);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    button.set_enabled(false);
    button.set_enabled(true);
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(calls == 1);
    click(document);
    REQUIRE(calls == 2);
}
