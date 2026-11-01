#include "render_document_frames.hpp"

#include <centui/backends/sdl.hpp>
#include <centui/platforms/sdl.hpp>

TEST_CASE("SDL platform and SDL renderer render document frames", "[integration][sdl]")
{
    centui::sdl::platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    centui::sdl::backend renderer{host};
    centui::tests::render_document_frames(host, renderer);
}
