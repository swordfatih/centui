#include <initializer_list>

#include <centui/document.hpp>
#include <centui/widgets/scroll.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone scroll header retains children and clamps offsets", "[widget][scroll][header]")
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

TEST_CASE("Flex scroll viewport fills the space beside a fixed sidebar across resizes", "[widget][scroll][yoga]")
{
    centui::style sidebar_style;
    sidebar_style.flex = centui::flex::all(0, 0, centui::px(248));
    auto sidebar = centui::container{sidebar_style};

    centui::style content_style;
    content_style.width = centui::percent(100);
    content_style.height = centui::px(200);
    content_style.padding = centui::edges::all(centui::px(30));
    auto content = centui::container{content_style};

    centui::style viewport_style;
    viewport_style.flex = centui::flex::all(1, 1, centui::px(0));
    viewport_style.height = centui::percent(100);
    auto viewport = centui::scroll{{content}, viewport_style};

    centui::style shell_style;
    shell_style.width = centui::percent(100);
    shell_style.height = centui::percent(100);
    shell_style.flex_direction = centui::direction::row;
    auto shell = centui::container{{sidebar, viewport}, shell_style};
    centui::document document{shell};

    for(auto surface: {centui::viewport{1440, 100}, centui::viewport{1100, 100}, centui::viewport{1440, 200, 2}})
    {
        (void)document.prepare(surface);
        const auto remaining = surface.logical_width() - 248;
        REQUIRE(sidebar->bounds().width == Catch::Approx(248));
        REQUIRE(viewport->bounds().x == Catch::Approx(248));
        REQUIRE(viewport->bounds().width == Catch::Approx(remaining));
        REQUIRE(content->bounds().width == Catch::Approx(remaining));
        REQUIRE(content->content_bounds().width == Catch::Approx(remaining - 60));
        REQUIRE(viewport->limit().x == Catch::Approx(0));
        REQUIRE(viewport->limit().y == Catch::Approx(100));
    }
}
