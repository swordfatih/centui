#include <centui/document.hpp>
#include <centui/widgets/range.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone range header exposes options and keyboard callbacks", "[widget][range][header]")
{
    float observed = 0;
    auto control = centui::range{{
        .minimum = 0,
        .maximum = 10,
        .step = 2,
        .value = 4,
        .on_change = [&](float value)
        {
            observed = value;
        },
    }};
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {200, 100});
    REQUIRE(control->value() == 6);
    REQUIRE(observed == 6);
}

TEST_CASE("Range dimensions share pointer and paint geometry", "[widget][range]")
{
    auto control = centui::range{{.preferred_size = {200, 40}, .thumb_width = 40, .track_thickness = 6}};
    centui::document document{control, {.system_font_discovery = false}};
    const auto& commands = document.prepare({200, 40}).commands;
    REQUIRE(control->bounds().width == 200);
    REQUIRE(control->bounds().height == 40);
    REQUIRE(commands.size() == 2);
    REQUIRE(commands[0].bounds.height == 6);
    REQUIRE(commands[1].bounds.width == 40);
    document.process_event({centui::input_event_type::pointer_pressed, 180, 20}, {200, 40});
    REQUIRE(control->value() == 1);
    REQUIRE_THROWS_AS(centui::range{centui::range_options{.thumb_width = -1}}, std::invalid_argument);
}
