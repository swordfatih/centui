#include <algorithm>
#include <string>
#include <string_view>

#include <centui/document.hpp>
#include <centui/widgets/container.hpp>
#include <centui/widgets/input.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone input header exposes edit and submit callbacks", "[widget][input][header]")
{
    std::string observed;
    int submits = 0;
    const auto observe = [&](std::string_view value)
    {
        observed = value;
    };

    const auto submit = [&]
    {
        ++submits;
    };

    auto control = centui::input{"A", {.on_change = observe, .on_submit = submit}};
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});

    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    document.process_event({.type = centui::input_event_type::text_entered, .text = "B"}, {200, 100});
    REQUIRE(control->value() == "AB");
    REQUIRE(observed == "AB");
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::enter}, {200, 100});
    REQUIRE(submits == 1);
}

TEST_CASE("Multiline Tab edits text while single-line and modified Tab navigate", "[widget][input][focus]")
{
    std::string observed;
    auto multiline = centui::input{
        "ABC",
        {.multiline = true,
         .on_change = [&](std::string_view value)
         {
             observed = value;
         }}
    };
    auto single = centui::input{"single"};
    centui::document document{
        centui::container{multiline, single},
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    (void)document.prepare({200, 100});
    REQUIRE(multiline->request_focus());

    SECTION("Tab replaces the selection, retains focus, and supports undo")
    {
        multiline->set_selection(1, 2);
        document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::tab}, {200, 100});
        REQUIRE(multiline->value() == "A\tC");
        REQUIRE(observed == "A\tC");
        REQUIRE(multiline->selection() == std::pair<std::size_t, std::size_t>{2, 2});
        REQUIRE(multiline->focused());
        document.process_event(
            {.type = centui::input_event_type::key_pressed, .code = centui::key::z, .mods = {.control = true}},
            {200, 100}
        );
        REQUIRE(multiline->value() == "ABC");
        REQUIRE(multiline->selection() == std::pair<std::size_t, std::size_t>{1, 2});
    }

    SECTION("Modified Tab leaves the text unchanged and moves focus")
    {
        centui::modifiers mods{};
        SECTION("Shift")
        {
            mods.shift = true;
        }
        SECTION("Control")
        {
            mods.control = true;
        }
        SECTION("Alt")
        {
            mods.alt = true;
        }
        SECTION("System")
        {
            mods.system = true;
        }
        document.process_event(
            {.type = centui::input_event_type::key_pressed, .code = centui::key::tab, .mods = mods},
            {200, 100}
        );
        REQUIRE(multiline->value() == "ABC");
        REQUIRE(single->focused());
    }

    SECTION("Single-line Tab moves focus without inserting text")
    {
        REQUIRE(single->request_focus());
        document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::tab}, {200, 100});
        REQUIRE(single->value() == "single");
        REQUIRE(multiline->focused());
    }
}

TEST_CASE("Input activity loss cancels composition while retaining logical focus", "[widget][input][focus]")
{
    auto control = centui::input{"A"};
    std::optional<centui::rect> host_area;
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto services = std::make_shared<centui::host_services>();
    services->set_text_input = [&](auto area)
    {
        host_area = area ? std::optional{area->area} : std::nullopt;
    };
    document.set_host_services(services);

    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    const auto initial_area = host_area;
    REQUIRE(initial_area.has_value());
    document.process_event(
        {.type = centui::input_event_type::composition, .text = "draft", .selection_start = 5},
        {200, 100}
    );
    REQUIRE(host_area != initial_area);
    document.process_event({centui::input_event_type::focus_lost}, {200, 100});
    REQUIRE(control->focused());
    REQUIRE_FALSE(control->focus_active());
    REQUIRE_FALSE(host_area.has_value());
    document.process_event({.type = centui::input_event_type::text_entered, .text = "ignored"}, {200, 100});
    REQUIRE(control->value() == "A");
    document.process_event({centui::input_event_type::focus_gained}, {200, 100});
    REQUIRE(host_area == initial_area);
    document.process_event({.type = centui::input_event_type::text_entered, .text = "B"}, {200, 100});
    REQUIRE(control->value() == "AB");
    REQUIRE(control->focus_active());
}

TEST_CASE("Mouse-focused input retains its caret and host session with no focus ring", "[widget][input][focus]")
{
    const centui::color ring_color{100, 210, 255, 255};
    auto control = centui::input{"A"};
    control->patch_style(
        {.width = centui::px(100), .height = centui::px(30), .focus = centui::focus{.color = ring_color}}
    );
    std::optional<centui::rect> host_area;
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto services = std::make_shared<centui::host_services>();
    services->set_text_input = [&](auto area)
    {
        host_area = area ? std::optional{area->area} : std::nullopt;
    };
    document.set_host_services(services);

    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE(control->focused());
    REQUIRE(control->focus_active());
    REQUIRE_FALSE(control->focus_visible());
    REQUIRE(host_area.has_value());
    const auto has_ring = [&](const auto& commands)
    {
        return std::ranges::any_of(
            commands,
            [&](const auto& command)
            {
                return command.color == ring_color;
            }
        );
    };
    const auto has_caret = [](const auto& commands)
    {
        return std::ranges::any_of(
            commands,
            [](const auto& command)
            {
                return !command.image && command.bounds.width == 1 && command.bounds.height > 0;
            }
        );
    };
    const auto& mouse_drawing = document.prepare({200, 100}).commands;
    REQUIRE(has_caret(mouse_drawing));
    REQUIRE_FALSE(has_ring(mouse_drawing));
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {200, 100});
    REQUIRE(control->focus_visible());
    REQUIRE(has_ring(document.prepare({200, 100}).commands));
    document.process_event({centui::input_event_type::pointer_pressed, 5, 5}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 5, 5}, {200, 100});
    REQUIRE_FALSE(control->focus_visible());
    document.process_event({.type = centui::input_event_type::text_entered, .text = "B"}, {200, 100});
    REQUIRE(control->focus_visible()); // Text entry also works without a separate physical key event.
    REQUIRE(has_caret(document.prepare({200, 100}).commands));
    REQUIRE(has_ring(document.prepare({200, 100}).commands));
    document.process_event({centui::input_event_type::focus_lost}, {200, 100});
    REQUIRE_FALSE(control->focus_visible());
    REQUIRE_FALSE(host_area.has_value());
    const auto& inactive_drawing = document.prepare({200, 100}).commands;
    REQUIRE_FALSE(has_caret(inactive_drawing));
    REQUIRE_FALSE(has_ring(inactive_drawing));
}
