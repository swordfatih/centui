#include "render_document_frames.hpp"

#include <centui/backends/bgfx.hpp>
#include <centui/platforms/sdl.hpp>

TEST_CASE("SDL platform and bgfx renderer render document frames", "[integration][sdl][bgfx]")
{
    centui::sdl::platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    centui::platform& platform = host;
    REQUIRE(platform.surface());
    centui::bgfx::backend renderer{platform};
    centui::tests::render_document_frames(host, renderer);
}
