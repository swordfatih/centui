#include <initializer_list>

#include <centui/document.hpp>
#include <centui/widgets/pressable.hpp>
#include <centui/widgets/scroll.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone scroll header retains children and clamps offsets", "[widget][scroll][header]")
{
    auto content = centui::container<>();
    content->patch_style({.height = centui::px(200)});
    auto control = centui::scroll{content};
    control->patch_style({.width = centui::px(100), .height = centui::px(50)});
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
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
    centui::document document{
        shell,
        {
            .system_font_discovery = false,
        }
    };
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});

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

TEST_CASE("Scrollbar thumbs paint above content and drag without activating covered controls", "[widget][scroll]")
{
    int clicks = 0;
    auto content = centui::pressable{
        centui::container{},
        {.on_click = [&]
         {
             ++clicks;
         }}
    };
    content->patch_style(
        {.width = centui::px(100),
         .height = centui::px(200),
         .background_color = centui::states_color::all({255, 0, 0, 255})}
    );
    auto control = centui::scroll{content};
    control->patch_style({.width = centui::px(100), .height = centui::px(50)});
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    const auto first = document.prepare({200, 100});
    REQUIRE(control->limit().y == Catch::Approx(150));
    REQUIRE(first.commands.size() >= 3);
    REQUIRE(first.commands.front().color == centui::color{255, 0, 0, 255});
    REQUIRE(first.commands.back().color.a < 255);
    const auto first_thumb_y = first.commands.back().bounds.y;

    document.process_event({centui::input_event_type::pointer_pressed, 95, 10}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 95, 100}, {200, 100});
    REQUIRE(control->offset().y == Catch::Approx(150));
    REQUIRE(document.prepare({200, 100}).commands.back().bounds.y > first_thumb_y);
    document.process_event({centui::input_event_type::pointer_released, 95, 100}, {200, 100});
    REQUIRE(clicks == 0);

    // The left side remains ordinary interactive content.
    document.process_event({centui::input_event_type::pointer_pressed, 10, 10}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 10, 10}, {200, 100});
    REQUIRE(clicks == 1);
}

TEST_CASE("Scrollbar tracks page content and capture dragging until release or cancellation", "[widget][scroll]")
{
    auto content = centui::container{};
    content->patch_style({.width = centui::px(100), .height = centui::px(200)});
    auto control = centui::scroll{content};
    control->patch_style({.width = centui::px(100), .height = centui::px(50)});
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    (void)document.prepare({200, 100});

    document.process_event({centui::input_event_type::pointer_pressed, 95, 45}, {200, 100});
    REQUIRE(control->offset().y == Catch::Approx(50));
    document.process_event({centui::input_event_type::pointer_released, 95, 45}, {200, 100});
    control->set_offset({0, 0});
    document.process_event({centui::input_event_type::pointer_pressed, 95, 10}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 195, 100}, {200, 100});
    REQUIRE(control->offset().y == Catch::Approx(150));
    document.process_event({centui::input_event_type::pointer_cancelled}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 95, 0}, {200, 100});
    REQUIRE(control->offset().y == Catch::Approx(150));
}

TEST_CASE("Horizontal and vertical scrollbars drag independently at scaled viewports", "[widget][scroll][dpi]")
{
    auto content = centui::container{};
    content->patch_style({.width = centui::px(400), .height = centui::px(200)});
    auto control = centui::scroll{content};
    control->patch_style({.width = centui::px(100), .height = centui::px(50)});
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    const centui::viewport surface{400, 200, 2};
    (void)document.prepare(surface);
    REQUIRE(control->limit().x == Catch::Approx(300));
    REQUIRE(control->limit().y == Catch::Approx(150));

    document.process_event({centui::input_event_type::pointer_pressed, 10, 45}, surface);
    document.process_event({centui::input_event_type::pointer_moved, 200, 45}, surface);
    REQUIRE(control->offset().x == Catch::Approx(300));
    REQUIRE(control->offset().y == 0);
    document.process_event({centui::input_event_type::pointer_released, 200, 45}, surface);
    document.process_event({centui::input_event_type::pointer_pressed, 95, 10}, surface);
    document.process_event({centui::input_event_type::pointer_moved, 95, 100}, surface);
    REQUIRE(control->offset().y == Catch::Approx(150));
    REQUIRE(control->offset().x == Catch::Approx(300));
}

TEST_CASE("Scrollbars disappear when content fits and tiny viewports remain finite", "[widget][scroll]")
{
    auto content = centui::container{};
    content->patch_style({.width = centui::px(100), .height = centui::px(200)});
    auto control = centui::scroll{content};
    control->patch_style({.width = centui::px(100), .height = centui::px(50)});
    centui::document document{
        control,
        {
            .system_font_discovery = false,
        }
    };
    REQUIRE_FALSE(document.prepare({200, 100}).commands.empty());
    content->patch_style({.height = centui::px(20)});
    REQUIRE(document.prepare({200, 100}).commands.empty());
    REQUIRE(control->limit().y == 0);

    content->patch_style({.width = centui::px(400), .height = centui::px(200)});
    control->patch_style({.width = centui::px(5), .height = centui::px(5)});
    (void)document.prepare({200, 100});
    document.process_event({centui::input_event_type::pointer_pressed, 2, 2}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 50, 50}, {200, 100});
    REQUIRE(control->offset().x == 0);
    REQUIRE(control->offset().y == 0);
}

TEST_CASE("Scrollbar dimensions control both drawing and hit testing", "[widget][scroll]")
{
    auto content = centui::container{};
    content->patch_style({.height = centui::px(200)});
    auto control = centui::scroll{content};
    control->patch_style({.width = centui::px(100), .height = centui::px(50)});
    control->set_scrollbars({.thickness = 20, .minimum_thumb = 10, .thumb_inset = 0, .corner_radius = 0});
    centui::document document{control, {.system_font_discovery = false}};
    const auto& commands = document.prepare({100, 50}).commands;
    REQUIRE(commands.size() == 2);
    REQUIRE(commands.front().bounds.x == 80);
    REQUIRE(commands.front().bounds.width == 20);
    document.process_event({centui::input_event_type::pointer_pressed, 85, 45}, {100, 50});
    REQUIRE(control->offset().y == 50);
    document.process_event({centui::input_event_type::pointer_released, 85, 45}, {100, 50});
    control->set_scrollbars({.thickness = 0});
    REQUIRE(document.prepare({100, 50}).commands.empty());
    REQUIRE_THROWS_AS(control->set_scrollbars({.thickness = -1}), std::invalid_argument);
    REQUIRE(control->scrollbars().thickness == 0);
}
