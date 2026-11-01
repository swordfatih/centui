#include <centui/centui.hpp>

#include CENTUI_PLATFORM_HEADER
#include CENTUI_BACKEND_HEADER

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Platform and renderer render four document frames", "[integration]")
{
    using Platform = CENTUI_PLATFORM_TYPE;
    using Renderer = CENTUI_BACKEND_TYPE;

    Platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    Renderer renderer{host};
    centui::ui integration{host, renderer};

    auto root = centui::container{centui::text{"Integration test"}, centui::button{"Button"}};
    root->patch_style({.width = centui::percent(100), .height = centui::percent(100)});

    centui::document document{root, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});

    for(int frame = 0; frame < 4; ++frame)
    {
        CAPTURE(frame);
        REQUIRE(host.poll_events());
        REQUIRE(integration.frame(document));
    }
}
