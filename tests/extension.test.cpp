#include <functional>
#include <limits>
#include <stdexcept>
#include <variant>
#include <vector>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class extension_probe : public centui::widget
{
public:
    explicit extension_probe(centui::style appearance = {})
        : widget(appearance)
    {
        set_focusable();
    }

    using widget::set_child_offset;
    using widget::set_clip_children;
    using widget::set_focusable;
    using widget::set_interactive;

    std::vector<bool> focus_changes;
    std::vector<centui::input_event_type> events;
    std::function<void(bool)> focus_callback;
    bool handle_events = false;

protected:
    bool on_event(const centui::input_event& event) override
    {
        events.push_back(event.type);

        return handle_events;
    }

    void on_focus_changed(bool value) override
    {
        focus_changes.push_back(value);

        if(focus_callback)
        {
            focus_callback(value);
        }
    }
};

class counting_button : public centui::pressable_widget
{
public:
    using pressable_widget::pressable_widget;
    int activations = 0;

protected:
    void activate() override
    {
        ++activations;
        pressable_widget::activate();
    }
};

centui::style box(float width, float height)
{
    centui::style result;
    result.width = centui::px(width);
    result.height = centui::px(height);

    return result;
}

} // namespace

TEST_CASE("Widget child operations work detached and mounted", "[widget][extension]")
{
    auto root = centui::make_widget<centui::container_widget>(box(200, 100));
    auto child = centui::make_widget<centui::container_widget>();
    auto other = centui::make_widget<centui::container_widget>();
    root->append(child);
    REQUIRE(child->parent() == centui::handle<>{root});
    REQUIRE_THROWS_AS(root->append(child), std::invalid_argument);
    REQUIRE_THROWS_AS(child->append(root), std::invalid_argument);
    REQUIRE_THROWS_AS(other->remove(child), std::invalid_argument);
    root->remove(child);
    REQUIRE_FALSE(child->parent().valid());
    {
        centui::document document{root};
        root->append(child);
        REQUIRE(document.parent(child) == centui::handle<>{root});
        root->remove(child);
        REQUIRE(root->children().empty());
        root->append(child);
    }
    // Surviving subtrees return to detached operations without a document reference.
    root->remove(child);
    other->append(child);
    REQUIRE(child->parent() == centui::handle<>{other});
}

TEST_CASE("Composite callbacks edit their children through the widget API", "[widget][extension]")
{
    auto root = centui::make_widget<centui::container_widget>(box(200, 100));
    auto action = centui::make_widget<centui::pressable_widget>(box(80, 30));
    auto replacement = centui::make_widget<centui::container_widget>(box(80, 30));
    root->append(action);
    centui::document document{root};
    const auto weak_root = root.weak();
    const auto weak_action = action.weak();
    action->set_on_click(
        [weak_root, weak_action, replacement]
        {
            auto parent = weak_root.lock();
            parent->remove(weak_action.lock());
            parent->append(replacement);
            REQUIRE(parent->children().front() == weak_action.lock());
        }
    );
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(root->children().size() == 1);
    REQUIRE(root->children().front() == centui::handle<>{replacement});
    REQUIRE_FALSE(action->parent().valid());
}

TEST_CASE("Focus is independent of pointer interaction and reports invalidation", "[widget][focus]")
{
    auto first = centui::make_widget<extension_probe>(box(80, 30));
    auto second = centui::make_widget<extension_probe>(box(80, 30));
    REQUIRE_FALSE(first->request_focus());
    auto root = centui::make_widget<centui::container_widget>(box(200, 100));
    root->append(first);
    root->append(second);
    centui::document document{root};
    REQUIRE(first->request_focus());
    REQUIRE(first->focused());
    REQUIRE(second->request_focus());
    REQUIRE_FALSE(first->focused());
    REQUIRE(second->focused());
    REQUIRE(first->focus_changes == std::vector<bool>{true, false});
    second->set_focusable(false);
    REQUIRE_FALSE(second->focused());
    REQUIRE_FALSE(second->request_focus());
    second->set_focusable();
    REQUIRE(second->request_focus());
    root->set_enabled(false);
    REQUIRE(second->enabled());
    REQUIRE_FALSE(second->effectively_enabled());
    REQUIRE_FALSE(second->focused());
    REQUIRE_FALSE(second->request_focus());
    root->set_enabled(true);
    REQUIRE(second->request_focus());
    root->remove(second);
    REQUIRE_FALSE(second->focused());
    REQUIRE_FALSE(second->focus_changes.back());
}

TEST_CASE("Focus callbacks defer topology edits and recover from exceptions", "[widget][focus]")
{
    auto child = centui::make_widget<extension_probe>();
    auto root = centui::make_widget<centui::container_widget>(box(200, 100));
    root->append(child);
    centui::document document{root};
    const auto weak_child = child.weak();
    const auto weak_root = root.weak();
    child->focus_callback = [weak_child, weak_root](bool focused)
    {
        if(!focused)
        {
            return;
        }

        weak_root.lock()->remove(weak_child.lock());
        REQUIRE(weak_child.lock()->parent().valid());

        throw std::runtime_error("focus callback");
    };
    REQUIRE_THROWS_AS(child->request_focus(), std::runtime_error);
    REQUIRE(root->children().empty());
    REQUIRE_FALSE(child->focused());
    REQUIRE(child->focus_changes == std::vector<bool>{true, false});
    child->focus_callback = {};
    root->append(child);
    REQUIRE(child->request_focus());
}

TEST_CASE("Child clipping and offsets agree for drawing and pointer targeting", "[widget][geometry]")
{
    auto root = centui::make_widget<extension_probe>(box(40, 30));
    auto appearance = box(60, 20);
    appearance.position = centui::position::absolute;
    appearance.inset.left = centui::px(20);
    appearance.inset.top = centui::px(0);
    appearance.background_color = centui::states_color::all({255, 0, 0, 255});
    auto child = centui::make_widget<counting_button>(appearance);
    root->append(child);
    centui::document document{root};
    document.process_event({centui::input_event_type::pointer_moved, 50, 5}, {200, 100});
    REQUIRE(child->hovered());
    root->set_clip_children(true);
    REQUIRE_FALSE(child->hovered());
    root->set_child_offset({-20, 0});
    const auto& commands = document.prepare({200, 100}).commands;
    REQUIRE(child->bounds().x == 0);
    REQUIRE(child->content_bounds().x == 0);
    REQUIRE(commands.size() == 1);
    const auto& rectangle = std::get<centui::rectangle_command>(commands.front());
    REQUIRE(rectangle.bounds.x == 0);
    REQUIRE(rectangle.clip.width == 40);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    REQUIRE(child->pressed());
    document.process_event({centui::input_event_type::pointer_released, 50, 5}, {200, 100});
    REQUIRE(child->activations == 0);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(child->activations == 1);
    REQUIRE_THROWS_AS(root->set_child_offset({std::numeric_limits<float>::infinity(), 0}), std::invalid_argument);
}

TEST_CASE("Content buttons reuse activation without inheriting text", "[widget][button]")
{
    auto content = centui::make_widget<centui::container_widget>(box(20, 20));
    auto action = centui::make_widget<counting_button>(content, box(80, 30));
    int callbacks = 0;
    action->set_on_click(
        [&]
        {
            ++callbacks;
        }
    );
    centui::document document{action};
    REQUIRE(content->parent() == centui::handle<>{action});
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(action->activations == 1);
    REQUIRE(callbacks == 1);
}

TEST_CASE("Unhandled motion reaches ancestors while captured gestures stay local", "[widget][input]")
{
    auto root = centui::make_widget<extension_probe>(box(200, 100));
    auto child = centui::make_widget<extension_probe>(box(80, 30));
    root->set_interactive();
    child->set_interactive();
    root->handle_events = true;
    root->append(child);
    centui::document document{root};
    document.process_event({centui::input_event_type::pointer_moved, 5, 5}, {200, 100});
    REQUIRE(root->events.size() == 1);
    REQUIRE(child->events.size() == 1);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 150, 90}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 150, 90}, {200, 100});
    REQUIRE(root->events.size() == 1);
    REQUIRE(child->events.size() == 4);
}
