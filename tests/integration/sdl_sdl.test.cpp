#include "render_document_frames.hpp"

#include <centui/backends/sdl.hpp>
#include <centui/platforms/sdl.hpp>

TEST_CASE("SDL platform and SDL renderer render document frames", "[integration][sdl]")
{
    centui::sdl::platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    centui::sdl::backend renderer{host};
    centui::tests::render_document_frames(host, renderer);
}

TEST_CASE("SDL renderer device loss recreates an owned renderer without a reset event", "[integration][sdl]")
{
    centui::sdl::platform host{{.title = "CentUI recovery test", .width = 160, .height = 120}};
    centui::sdl::backend renderer{host};
    centui::ui ui{host, renderer};
    centui::document document{centui::document_options{.system_font_discovery = false}};
    REQUIRE(ui.frame(document, centui::frame_duration::zero()));

    const auto generation = host.state().surface_generation;
    SDL_Event lost{};
    lost.type = SDL_EVENT_RENDER_DEVICE_LOST;
    lost.render.windowID = SDL_GetWindowID(&host.native_window());
    ui.process_event(document, lost);

    REQUIRE(host.state().drawable);
    REQUIRE(host.state().surface_generation == generation + 1);
    REQUIRE(SDL_GetRenderer(&host.native_window()) != nullptr);
    REQUIRE(ui.frame(document, centui::frame_duration::zero()));
    REQUIRE(ui.frame(document, centui::frame_duration::zero()));
}
