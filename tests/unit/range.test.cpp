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
    centui::document document{control};
    document.set_system_font_discovery(false);
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::right}, {200, 100});
    REQUIRE(control->value() == 6);
    REQUIRE(observed == 6);
}
