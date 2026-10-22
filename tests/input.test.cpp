#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include "test_font.hpp"

import centui.widgets.input;

TEST_CASE("Standalone input import exposes edit and submit callbacks", "[widget][input][module]")
{
    std::string observed;
    int submits = 0;
    auto control = centui::input{"A", {.on_change = [&](std::string_view value) { observed = value; },
                                        .on_submit = [&] { ++submits; }}};
    centui::document document{control};
    document.set_font(centui::font{test_font_path()});
    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    document.process_event({.type = centui::input_event_type::text_entered, .text = "B"}, {200, 100});
    REQUIRE(control->value() == "AB");
    REQUIRE(observed == "AB");
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::enter}, {200, 100});
    REQUIRE(submits == 1);
}
