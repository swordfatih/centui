#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

namespace
{

class focus_probe : public centui::widget
{
public:
    focus_probe()
    {
        set_focusable();
        set_interactive();
        patch_style(
            {.width = centui::px(40), .height = centui::px(30), .flex = centui::flex::all(0, 0, centui::automatic())}
        );
    }

    using widget::set_clip_children;
    using widget::set_focusable;
    std::function<void(std::string)> notify;
    std::vector<centui::input_event_type> events;
    bool text_session = false;

protected:
    void on_focus_changed(bool value) override
    {
        REQUIRE(focused() == value);
        if(notify)
        {
            notify(value ? "logical+" : "logical-");
        }
    }

    void on_focus_activity_changed(bool value) override
    {
        REQUIRE(focused());
        REQUIRE(focus_active() == value);
        if(notify)
        {
            notify(value ? "active+" : "active-");
        }
    }

    bool on_event(const centui::input_event& event) override
    {
        events.push_back(event.type);
        return false;
    }

    std::optional<centui::rect> text_input_area() const override
    {
        return text_session ? std::optional{bounds()} : std::nullopt;
    }
};

auto probe()
{
    return centui::make_widget<focus_probe>();
}

} // namespace

TEST_CASE("Focus activity has ordered notifications and survives inactive transfers", "[focus]")
{
    auto a = probe();
    auto b = probe();
    centui::document doc{centui::container{a, b}};
    std::vector<std::string> changes;
    a->notify = [&](auto value)
    {
        changes.push_back("a:" + value);
    };
    b->notify = [&](auto value)
    {
        changes.push_back("b:" + value);
    };
    REQUIRE(a->request_focus());
    changes.clear();
    REQUIRE(b->request_focus());
    REQUIRE(changes == std::vector<std::string>{"a:active-", "a:logical-", "b:logical+", "b:active+"});
    changes.clear();
    REQUIRE(b->request_focus());
    REQUIRE(changes.empty());
    doc.process_event({centui::input_event_type::focus_lost}, {100, 100});
    REQUIRE(doc.focused_widget() == centui::handle<>{b});
    REQUIRE_FALSE(b->focus_active());
    REQUIRE(changes == std::vector<std::string>{"b:active-"});
    changes.clear();
    REQUIRE(a->request_focus());
    REQUIRE(changes == std::vector<std::string>{"b:logical-", "a:logical+"});
    changes.clear();
    doc.process_event({centui::input_event_type::focus_gained}, {100, 100});
    REQUIRE(changes == std::vector<std::string>{"a:active+"});
    doc.process_event({centui::input_event_type::focus_lost}, {0, 0});
    a->set_enabled(false);
    doc.process_event({centui::input_event_type::focus_gained}, {100, 100});
    REQUIRE_FALSE(doc.focused_widget().valid());
}

TEST_CASE("Window activity stops host ownership and suppresses active drawing", "[focus][host][paint]")
{
    auto a = probe();
    a->text_session = true;
    std::vector<std::optional<centui::rect>> calls;
    centui::document doc{a};
    auto host = std::make_shared<centui::host_services>();
    host->text_input = [&](auto area)
    {
        calls.push_back(area);
    };
    doc.set_host_services(host);
    (void)doc.prepare({100, 100});
    REQUIRE(a->request_focus());
    REQUIRE(calls.back().has_value());
    REQUIRE_FALSE(doc.prepare({100, 100}).commands.empty());
    doc.process_event({centui::input_event_type::focus_lost}, {0, 0});
    REQUIRE_FALSE(calls.back().has_value());
    REQUIRE(a->focused());
    REQUIRE(doc.prepare({100, 100}).commands.empty());
    doc.process_event({centui::input_event_type::focus_gained}, {100, 100});
    REQUIRE(calls.back().has_value());
}

TEST_CASE("Traversal crosses ordinary containers and wraps in both directions", "[focus][traversal]")
{
    auto a = probe();
    auto b = probe();
    auto c = probe();
    centui::document empty;
    REQUIRE_FALSE(empty.focus_next());
    auto group = centui::container{a, b};
    centui::document doc{centui::container{group, c}};
    REQUIRE(doc.focus_next(true));
    REQUIRE(c->focused());
    REQUIRE(doc.focus_next());
    REQUIRE(a->focused());
    REQUIRE(doc.focus_next());
    REQUIRE(b->focused());
    REQUIRE(doc.focus_next());
    REQUIRE(c->focused());
    b->set_enabled(false);
    REQUIRE(doc.focus_next(true));
    REQUIRE(a->focused());
    a->patch_style({.visibility = centui::visibility::hidden});
    REQUIRE_FALSE(doc.focused_widget().valid());
    REQUIRE(doc.focus_next());
    REQUIRE(c->focused());
    c->patch_style({.display = centui::display::contents});
    REQUIRE_FALSE(c->request_focus());
}

TEST_CASE("Modal entry fallback, nested unwind and restoration belong to the document", "[focus][modal]")
{
    auto before = probe();
    auto a = probe();
    auto b = probe();
    auto inner = centui::container{b};
    auto outer = centui::container{a, inner};
    centui::document doc{centui::container{before, outer}};
    REQUIRE(before->request_focus());
    doc.enter_modal(outer, a);
    REQUIRE(a->focused());
    REQUIRE_FALSE(before->request_focus());
    doc.enter_modal(outer);
    REQUIRE(a->focused());
    doc.enter_modal(inner);
    REQUIRE(b->focused());
    REQUIRE_THROWS_AS(doc.enter_modal(outer), std::invalid_argument);
    REQUIRE_THROWS_AS(doc.enter_modal(before), std::invalid_argument);
    REQUIRE(doc.focus_next());
    REQUIRE(b->focused());
    // Non-top closure restores only the lowest removed entry's target.
    doc.leave_modal(outer);
    REQUIRE(before->focused());
    doc.leave_modal(inner);
    doc.leave_modal(outer);
    doc.leave_modal({});
    REQUIRE(before->focused());
    doc.enter_modal(outer, before); // Ineligible initial target falls back to a.
    REQUIRE(a->focused());
    before->set_enabled(false);
    doc.leave_modal(outer);
    REQUIRE_FALSE(doc.focused_widget().valid());
}

TEST_CASE("Empty modals retain restrictions and modal root eligibility is checked", "[focus][modal]")
{
    auto background = probe();
    auto empty = centui::container{};
    auto root_fallback = probe();
    centui::document doc{centui::container{background, empty, root_fallback}};
    doc.enter_modal(empty);
    REQUIRE_FALSE(doc.focused_widget().valid());
    REQUIRE_FALSE(doc.focus_next());
    REQUIRE_FALSE(background->request_focus());
    doc.leave_modal(empty);
    doc.enter_modal(root_fallback);
    REQUIRE(root_fallback->focused());
    doc.leave_modal(root_fallback);
    auto detached = probe();
    centui::document foreign{probe()};
    REQUIRE_THROWS_AS(doc.enter_modal(detached), std::invalid_argument);
    REQUIRE_THROWS_AS(doc.enter_modal(foreign.root()), std::invalid_argument);
    REQUIRE_THROWS_AS(doc.enter_modal({}), std::invalid_argument);
    empty->set_enabled(false);
    REQUIRE_THROWS_AS(doc.enter_modal(empty), std::invalid_argument);
    empty->set_enabled(true);
    empty->patch_style({.visibility = centui::visibility::hidden});
    REQUIRE_THROWS_AS(doc.enter_modal(empty), std::invalid_argument);
}

TEST_CASE("Modal topology and visibility changes unwind at the boundary", "[focus][modal]")
{
    for(int mutation = 0; mutation != 3; ++mutation)
    {
        DYNAMIC_SECTION("mutation " << mutation)
        {
            auto before = probe();
            auto inside = probe();
            auto modal = centui::container{inside};
            auto root = centui::container{before, modal};
            centui::document doc{root};
            REQUIRE(before->request_focus());
            doc.enter_modal(modal);
            if(mutation == 0)
            {
                root->remove(modal);
            }
            if(mutation == 1)
            {
                modal->set_enabled(false);
            }
            if(mutation == 2)
            {
                modal->patch_style({.visibility = centui::visibility::hidden});
            }
            REQUIRE(before->focused());
            REQUIRE_FALSE(inside->focused());
        }
    }
}

TEST_CASE("Modal routing cancels capture and bounds keyboard, pointer and wheel bubbling", "[focus][modal][input]")
{
    auto background = probe();
    auto inside = probe();
    auto modal = probe();
    modal->set_focusable(false);
    modal->append(inside);
    auto root = probe();
    root->set_focusable(false);
    root->patch_style({.width = centui::px(100), .height = centui::px(100)});
    root->append(background);
    root->append(modal);
    centui::document doc{root};
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    REQUIRE(background->pressed());
    doc.enter_modal(modal);
    REQUIRE_FALSE(background->pressed());
    REQUIRE(background->events.back() == centui::input_event_type::pointer_cancelled);
    const auto background_events = background->events.size();
    root->events.clear();
    doc.process_event({centui::input_event_type::pointer_moved, 5, 5}, {100, 100});
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    REQUIRE(background->events.size() == background_events);
    doc.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::enter}, {100, 100});
    const auto box = inside->bounds();
    doc.process_event({centui::input_event_type::pointer_moved, box.x + 1, box.y + 1}, {100, 100});
    doc.process_event(
        {.type = centui::input_event_type::wheel, .x = box.x + 1, .y = box.y + 1, .delta_y = 10},
        {100, 100}
    );
    REQUIRE_FALSE(modal->events.empty());
    REQUIRE(root->events.empty());
    doc.leave_modal(modal);
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    REQUIRE(background->events.size() == background_events);
}

TEST_CASE("Focus notification failures clean up each stage and retain the original exception", "[focus][callbacks]")
{
    for(const std::string stage: {"old-active-", "old-logical-", "new-logical+", "new-active+"})
    {
        DYNAMIC_SECTION(stage)
        {
            auto a = probe();
            auto b = probe();
            a->text_session = b->text_session = true;
            auto root = centui::container{a, b};
            std::optional<centui::rect> host_area;
            centui::document doc{root};
            auto host = std::make_shared<centui::host_services>();
            host->text_input = [&](auto area)
            {
                host_area = area;
            };
            doc.set_host_services(host);
            (void)doc.prepare({100, 100});
            doc.enter_modal(root, a);
            REQUIRE(host_area.has_value());
            const auto fail = [&](std::string value)
            {
                if(value == stage)
                {
                    doc.remove(b);
                    a->request_focus(); // Queued focus work must be discarded.
                    throw std::runtime_error("original notification");
                }
            };
            a->notify = [&](auto value)
            {
                fail("old-" + value);
            };
            b->notify = [&](auto value)
            {
                fail("new-" + value);
            };
            REQUIRE_THROWS_WITH(b->request_focus(), "original notification");
            REQUIRE_FALSE(doc.focused_widget().valid());
            REQUIRE_FALSE(a->focused());
            REQUIRE_FALSE(b->focused());
            REQUIRE_FALSE(a->focus_active());
            REQUIRE_FALSE(b->focus_active());
            REQUIRE_FALSE(a->focus_visible());
            REQUIRE_FALSE(b->focus_visible());
            REQUIRE_FALSE(host_area.has_value());
            REQUIRE(root->children().size() == 1);
            a->notify = {};
            REQUIRE(doc.focus_next());
            REQUIRE(a->focused());
        }
    }
}

TEST_CASE("Alternating callbacks share a bounded transaction and leave the next event usable", "[focus][callbacks]")
{
    auto a = probe();
    auto b = probe();
    auto outside = probe();
    auto modal = centui::container{a, b};
    centui::document doc{centui::container{outside, modal}};
    doc.enter_modal(modal, a);
    int notifications = 0;
    a->notify = [&](auto value)
    {
        if(value == "logical+")
        {
            ++notifications;
            b->request_focus();
        }
    };
    b->notify = [&](auto value)
    {
        if(value == "logical+")
        {
            ++notifications;
            a->request_focus();
        }
    };
    REQUIRE_THROWS_AS(b->request_focus(), std::logic_error);
    REQUIRE(notifications <= 64);
    REQUIRE_FALSE(a->focused());
    REQUIRE_FALSE(b->focused());
    REQUIRE_FALSE(doc.focused_widget().valid());
    REQUIRE_FALSE(outside->request_focus()); // Valid modality survives emergency cleanup.
    a->notify = b->notify = {};
    REQUIRE(a->request_focus());
}

TEST_CASE("Callback mutations reconcile eligibility without duplicate ownership", "[focus][callbacks]")
{
    auto a = probe();
    auto b = probe();
    centui::document doc{centui::container{a, b}};
    a->notify = [&](auto value)
    {
        if(value == "logical+")
        {
            a->set_enabled(false);
            b->request_focus();
        }
    };
    REQUIRE(a->request_focus());
    REQUIRE_FALSE(a->focused());
    REQUIRE(b->focus_active());
    doc.clear_focus();
    REQUIRE_FALSE(b->focused());
}

TEST_CASE("Focus ring follows keyboard and mouse input without changing ownership", "[focus][paint]")
{
    auto a = probe();
    auto label = centui::container{};
    label->patch_style({.width = centui::px(20), .height = centui::px(10)});
    a->append(label);
    centui::document doc{a};
    std::vector<std::string> notifications;
    a->notify = [&](auto value)
    {
        notifications.push_back(value);
    };
    REQUIRE(a->request_focus());
    REQUIRE(a->focus_visible());
    const auto initial_notifications = notifications;
    const auto normal = doc.prepare({100, 100}).commands.size();
    REQUIRE(normal > 0);
    const auto geometry = doc.statistics();
    doc.process_event({centui::input_event_type::pointer_moved, 5, 5}, {100, 100});
    REQUIRE(a->focus_visible());
    REQUIRE(doc.prepare({100, 100}).commands.size() == normal);
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    REQUIRE(a->focused());
    REQUIRE(a->focus_active());
    REQUIRE_FALSE(a->focus_visible());
    REQUIRE(a->request_focus());
    doc.process_event({centui::input_event_type::pointer_moved, 6, 6}, {100, 100});
    REQUIRE_FALSE(a->focus_visible());
    REQUIRE(doc.prepare({100, 100}).commands.empty());
    doc.process_event({.type = centui::input_event_type::key_released, .code = centui::key::right}, {100, 100});
    REQUIRE_FALSE(a->focus_visible());
    doc.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {100, 100});
    REQUIRE(a->focus_visible());
    REQUIRE(a->pressed()); // Pressed/hovered appearance does not override keyboard indication.
    REQUIRE(doc.prepare({100, 100}).commands.size() == normal);
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    doc.process_event({.type = centui::input_event_type::wheel, .x = 5, .y = 5, .delta_y = 10}, {100, 100});
    REQUIRE(a->focus_visible());
    REQUIRE(doc.prepare({100, 100}).commands.size() == normal);
    REQUIRE_FALSE(label->focus_active());
    REQUIRE_FALSE(label->focus_visible());
    REQUIRE(notifications == initial_notifications);
    REQUIRE(doc.statistics().layout_calculations == geometry.layout_calculations);
    REQUIRE(doc.statistics().measurement_calls == geometry.measurement_calls);
    REQUIRE(doc.statistics().geometry_callbacks == geometry.geometry_callbacks);
    const auto before = doc.statistics();
    a->patch_style({.focus = centui::focus{.width = 0}});
    REQUIRE(a->focus_visible()); // The query describes interaction state, independent of theme styling.
    REQUIRE(doc.prepare({100, 100}).commands.empty());
    REQUIRE(doc.statistics().layout_calculations == before.layout_calculations);
    REQUIRE(doc.statistics().measurement_calls == before.measurement_calls);
}

TEST_CASE("Window activation preserves the last focus indication mode", "[focus][visibility]")
{
    for(bool keyboard: {false, true})
    {
        DYNAMIC_SECTION("keyboard mode " << keyboard)
        {
            auto a = probe();
            centui::document doc{a};
            REQUIRE(a->request_focus());
            if(!keyboard)
            {
                doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
                doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
            }
            REQUIRE(a->focus_visible() == keyboard);
            doc.process_event({centui::input_event_type::focus_lost}, {100, 100});
            REQUIRE(a->focused());
            REQUIRE_FALSE(a->focus_active());
            REQUIRE_FALSE(a->focus_visible());
            doc.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {100, 100});
            doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
            doc.process_event({centui::input_event_type::focus_gained}, {100, 100});
            REQUIRE(a->focus_active());
            REQUIRE(a->focus_visible() == keyboard);
        }
    }
}

TEST_CASE(
    "Programmatic focus and modals inherit input mode while traversal shows indication",
    "[focus][visibility][modal]"
)
{
    auto a = probe();
    auto b = probe();
    auto modal = centui::container{b};
    centui::document doc{centui::container{a, modal}};
    REQUIRE(a->request_focus());
    REQUIRE(a->focus_visible()); // Programmatic focus starts visible before any pointer interaction.
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    doc.enter_modal(modal);
    REQUIRE(b->focus_active());
    REQUIRE_FALSE(b->focus_visible());
    doc.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {100, 100});
    REQUIRE(b->focus_visible());
    doc.leave_modal(modal);
    REQUIRE(a->focus_visible());
    REQUIRE_FALSE(b->focus_visible());
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    REQUIRE(b->request_focus());
    REQUIRE_FALSE(b->focus_visible());
    doc.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::tab}, {100, 100});
    REQUIRE(a->focus_visible());
    doc.process_event(
        {.type = centui::input_event_type::key_pressed, .code = centui::key::tab, .mods = {.shift = true}},
        {100, 100}
    );
    REQUIRE(b->focus_visible());
    doc.clear_focus();
    REQUIRE_FALSE(a->focus_visible());
    REQUIRE_FALSE(b->focus_visible());
}

TEST_CASE("Focus indication input mode belongs to each document", "[focus][visibility]")
{
    auto a = probe();
    auto b = probe();
    centui::document first{a};
    centui::document second{b};
    REQUIRE(a->request_focus());
    REQUIRE(b->request_focus());
    first.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    REQUIRE_FALSE(a->focus_visible());
    REQUIRE(b->focus_visible());
    REQUIRE(first.focus_next()); // Even single-target traversal reveals keyboard focus without a transfer.
    REQUIRE(a->focus_visible());
    first.remove(a);
    REQUIRE_FALSE(a->focus_visible());
}

TEST_CASE("Focus indicator composes and validates as a paint-only style", "[focus][style]")
{
    struct ring_width
    {
        struct
        {
            float width = 3;
        } focus;
    };

    struct ring_inset
    {
        struct
        {
            float inset = 4;
        } focus;
    };

    const auto composed = centui::style::resolve<ring_width, ring_inset>();
    REQUIRE(composed.focus.width == 3);
    REQUIRE(composed.focus.inset == 4);
    auto patched = centui::style_patch{.focus = centui::focus{.width = 0}}.apply(composed);
    REQUIRE(patched.focus.width == 0);
    REQUIRE_FALSE(patched.changes_from(composed).layout);
    REQUIRE(patched.changes_from(composed).paint);
    for(float value: {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        patched.focus.width = value;
        REQUIRE_THROWS_AS(patched.validate(), std::invalid_argument);
        patched.focus.width = 1;
        patched.focus.inset = value;
        REQUIRE_THROWS_AS(patched.validate(), std::invalid_argument);
    }
}

TEST_CASE("Nested modal closure falls back within the remaining restriction", "[focus][modal]")
{
    auto outside = probe();
    auto a = probe();
    auto b = probe();
    auto inner = centui::container{b};
    auto outer = centui::container{a, inner};
    centui::document doc{centui::container{outside, outer}};
    REQUIRE(outside->request_focus());
    doc.enter_modal(outer, a);
    doc.enter_modal(inner, b);
    a->set_enabled(false);
    doc.leave_modal(inner);
    REQUIRE(b->focused()); // First eligible descendant under the remaining outer modal.
    REQUIRE_FALSE(outside->request_focus());
    doc.leave_modal(outer);
    REQUIRE(outside->focused());
}

TEST_CASE("Detached popup open intent mounts without geometry and unmount restores centrally", "[focus][popup]")
{
    auto outside = probe();
    auto inside = probe();
    auto popup = centui::popup{inside};
    popup->open();
    auto root = centui::container{outside};
    centui::document doc{root};
    REQUIRE(outside->request_focus());
    root->append(popup); // No usable surface and no on_geometry call required.
    REQUIRE(inside->focused());
    REQUIRE_FALSE(outside->request_focus());
    root->remove(popup);
    REQUIRE(outside->focused());
    root->append(popup);
    REQUIRE(inside->focused());
    popup->close();
    REQUIRE(outside->focused());
    doc.set_root(probe());
    REQUIRE_FALSE(outside->focused());
    REQUIRE_FALSE(inside->focused());
    REQUIRE(doc.focus_next());
}

TEST_CASE("Background pressable activation is blocked and cancelled gestures never resume", "[focus][modal][pressable]")
{
    int calls = 0;
    auto button = centui::pressable{
        centui::container{},
        {.on_click = [&]
         {
             ++calls;
         }}
    };
    button->patch_style({.width = centui::px(40), .height = centui::px(30)});
    auto modal = centui::container{};
    modal->patch_style({.width = centui::px(40), .height = centui::px(30)});
    centui::document doc{centui::container{button, modal}};
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    doc.enter_modal(modal);
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    doc.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {100, 100});
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    REQUIRE(calls == 0);
    doc.leave_modal(modal);
    doc.process_event({centui::input_event_type::pointer_released, 5, 5}, {100, 100});
    REQUIRE(calls == 0);
    REQUIRE(button->request_focus());
    doc.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::space}, {100, 100});
    REQUIRE(calls == 1); // Pressables activate on nonrepeat press, never on release.
    doc.enter_modal(modal);
    doc.process_event({.type = centui::input_event_type::key_released, .code = centui::key::space}, {100, 100});
    doc.leave_modal(modal);
    doc.process_event({.type = centui::input_event_type::key_released, .code = centui::key::space}, {100, 100});
    REQUIRE(calls == 1);
}
