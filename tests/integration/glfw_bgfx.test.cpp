#include "render_document_frames.hpp"

#include <centui/backends/bgfx.hpp>
#include <centui/platforms/glfw.hpp>

TEST_CASE("GLFW platform and bgfx renderer render document frames", "[integration][glfw][bgfx]")
{
    centui::glfw::platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    centui::platform& platform = host;
    REQUIRE(platform.surface());
    centui::bgfx::backend renderer{platform};
    centui::tests::render_document_frames(host, renderer);
}
