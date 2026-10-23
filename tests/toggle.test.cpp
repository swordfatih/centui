#include "test_font.hpp"

#include <string>

#include <centui/widgets/toggle.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone toggle header preserves composed label and activation", "[widget][toggle][header]")
{
    bool observed = false;
    auto control = centui::toggle{
        "Toggle",
        {
            .on_change = [&](bool value)
            {
                observed = value;
            },
        }
    };
    centui::document document{control};
    document.set_font(centui::font{test_font_path()});
    (void)document.prepare({200, 100});
    REQUIRE(control->children().size() == 1);
    REQUIRE(control->request_focus());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::space}, {200, 100});
    REQUIRE(control->checked());
    REQUIRE(observed);
    control->set_text("Enabled");
    REQUIRE(control->value() == "Enabled");
    REQUIRE(document.pending_changes().layout);
}
