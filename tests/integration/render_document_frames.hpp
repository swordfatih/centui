#pragma once

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace centui::tests
{

template <typename Platform>
void render_document_frames(Platform& host, centui::renderer& renderer)
{
    centui::ui integration{host, renderer};
    auto root = centui::container{centui::text{"Integration test"}, centui::button{"Button"}};
    root->patch_style({.width = centui::percent(100), .height = centui::percent(100)});

    centui::document document{root, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    document.set_style({.font = centypo::font{"Inter"}});

    for(int frame = 0; frame < 4; ++frame)
    {
        CAPTURE(frame);
        REQUIRE(host.poll_events());
        REQUIRE(integration.frame(document));
    }

    auto state = host.state();
    state.drawable = false;
    integration.process_event(document, platform_event{surface_event{surface_change::unavailable, state}});
    REQUIRE_NOTHROW(renderer.end_frame());
    REQUIRE_FALSE(integration.frame(document));
    ++state.surface_generation;
    integration.process_event(document, platform_event{surface_event{surface_change::resources_invalidated, state}});
    REQUIRE_FALSE(integration.frame(document));
    state.drawable = true;
    ++state.surface_generation;
    integration.process_event(document, platform_event{surface_event{surface_change::available, state}});
    REQUIRE(integration.frame(document));
    ++state.surface_generation;
    integration.process_event(document, platform_event{surface_event{surface_change::resources_invalidated, state}});
    REQUIRE(integration.frame(document));
}

} // namespace centui::tests
