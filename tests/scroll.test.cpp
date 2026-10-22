#include <initializer_list>

#include <catch2/catch_test_macros.hpp>

import centui.widgets.scroll;

TEST_CASE("Standalone scroll import retains children and clamps offsets", "[widget][scroll][module]")
{
    auto content = centui::container<>();
    content.patch_style({.height = centui::px(200)});
    auto control = centui::scroll{content};
    control.patch_style({.width = centui::px(100), .height = centui::px(50)});
    centui::document document{control};
    (void)document.prepare({200, 100});
    REQUIRE(content->parent() == centui::handle<>{control});
    REQUIRE(control->limit().y > 0);
    control->set_offset({0, 1000});
    REQUIRE(control->offset().y == control->limit().y);
    control->set_offset({0, -10});
    REQUIRE(control->offset().y == 0);
}
