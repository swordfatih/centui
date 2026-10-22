#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include "test_font.hpp"

import centui.widgets.selection;

TEST_CASE("Standalone selection import exposes items options and callbacks", "[widget][selection][module]")
{
    std::size_t observed = 0;
    auto control = centui::selection{{"First", "Second"},
                                     {.selected = 0, .on_change = [&](std::size_t value) { observed = value; }}};
    centui::document document{control};
    document.set_font(centui::font{test_font_path()});
    (void)document.prepare({200, 100});
    REQUIRE(control->request_focus());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::down}, {200, 100});
    REQUIRE(control->selected() == 1);
    REQUIRE(observed == 1);
}
