#include <initializer_list>
#include <variant>

#include <centui/widgets/pressable.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct action_style
{
    centui::length width = centui::px(100);
    centui::length height = centui::px(60);
};

void click(centui::document& document, float y)
{
    document.process_event({centui::input_event_type::pointer_pressed, 5, y}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, y}, {200, 100});
}

} // namespace

TEST_CASE("Pressables activate through nested content and multiple children", "[widget][pressable]")
{
    auto first = centui::container<>();
    auto second = centui::container<>();
    first.patch_style({.height = centui::px(20)});
    second.patch_style({.height = centui::px(20)});
    int calls = 0;
    centui::handle<centui::pressable_widget> action;
    SECTION("Single nested content argument")
    {
        action = centui::pressable<action_style>{
            centui::container{first, second},
            {
                .on_click = [&]
                {
                    ++calls;
                },
            }
        };
    }

    SECTION("Child list argument")
    {
        action = centui::pressable<action_style>{
            {first, second},
            {
                .on_click = [&]
                {
                    ++calls;
                },
            }
        };
    }

    centui::document document{action};
    click(document, 5);
    click(document, 25);
    REQUIRE(calls == 2);
    REQUIRE(action->focused());
    REQUIRE_FALSE(first->focused());
    REQUIRE_FALSE(second->focused());
    action->set_enabled(false);
    click(document, 5);
    REQUIRE(calls == 2);
}

TEST_CASE("Pressable keyboard activation ignores repeats and disabled state", "[widget][pressable]")
{
    int calls = 0;
    auto action = centui::pressable<action_style>{
        centui::container<>{},
        {
            .on_click = [&]
            {
                ++calls;
            },
        }
    };
    centui::document document{action};
    (void)document.prepare({200, 100});
    REQUIRE(action->request_focus());

    for(const auto key: {centui::key::enter, centui::key::space})
    {
        const auto previous = calls;
        document.process_event({.type = centui::input_event_type::key_pressed, .code = key}, {200, 100});
        REQUIRE(calls == previous + 1);
        document.process_event(
            {.type = centui::input_event_type::key_pressed, .code = key, .repeat = true},
            {200, 100}
        );
        document.process_event({.type = centui::input_event_type::key_released, .code = key}, {200, 100});
        REQUIRE(calls == previous + 1);
    }

    action.set_enabled(false);
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::enter}, {200, 100});
    REQUIRE(calls == 2);
}

TEST_CASE("Noninteractive descendants inherit pressable visual states", "[widget][pressable][style]")
{
    auto colors = centui::states_color::all({10, 0, 0, 255});
    colors.hover = {20, 0, 0, 255};
    colors.pressed = {30, 0, 0, 255};
    colors.focused = {40, 0, 0, 255};
    colors.disabled = {50, 0, 0, 255};
    auto content = centui::container<>();
    content.patch_style({.height = centui::px(20), .background_color = colors});
    auto action = centui::pressable<action_style>{centui::container{content}};
    centui::document document{action};
    const auto color = [&]
    {
        const auto& commands = document.prepare({200, 100}).commands;
        REQUIRE(commands.size() == 1);

        return std::get<centui::rectangle_command>(commands.front()).color;
    };
    REQUIRE(color() == colors.normal);
    document.process_event({centui::input_event_type::pointer_moved, 5, 5}, {200, 100});
    REQUIRE(color() == colors.hover);
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    REQUIRE(color() == colors.pressed);
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_left}, {200, 100});
    REQUIRE(color() == colors.focused);
    action.set_enabled(false);
    REQUIRE(color() == colors.disabled);
}
