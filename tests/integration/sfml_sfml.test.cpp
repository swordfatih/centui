#include "render_document_frames.hpp"

#include <centui/backends/sfml.hpp>
#include <centui/platforms/sfml.hpp>

TEST_CASE("SFML platform and SFML renderer render document frames", "[integration][sfml]")
{
    centui::sfml::platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    centui::sfml::backend renderer{host};
    centui::tests::render_document_frames(host, renderer);
}
