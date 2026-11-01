#include <centui/backends/bgfx.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("bgfx batches quads and submits beyond transient buffer capacity", "[renderer][bgfx]")
{
    // Run the no-op renderer on this thread, without a native window.
    bgfx::renderFrame();
    bgfx::Init init;
    init.type = bgfx::RendererType::Noop;
    init.limits.maxTransientVbSize = 1024;
    init.limits.maxTransientIbSize = 1024;
    REQUIRE(bgfx::init(init));

    struct shutdown
    {
        ~shutdown()
        {
            bgfx::shutdown();
        }
    } shutdown;

    const centui::viewport viewport{64, 64};
    centui::bgfx::backend renderer{viewport};
    centui::draw_list list{viewport};
    list.commands.resize(20000, centui::draw_quad{.bounds = {0, 0, 1, 1}});
    renderer.draw(list);
    bgfx::frame();
    REQUIRE(bgfx::getStats()->numDrawCallsPeak == 2);

    const auto image = std::make_shared<const centui::bitmap>(centui::bitmap{
        1,
        1,
        {255, 255, 255, 255},
        centui::pixel_format::rgba,
    });
    list.commands.resize(8);
    list.commands.push_back({.bounds = {0, 0, 1, 1}, .image = image});
    list.commands.push_back({.bounds = {1, 0, 1, 1}, .image = image});
    list.commands.push_back({.bounds = {2, 0, 1, 1}});
    renderer.draw(list);
    bgfx::frame();
    REQUIRE(bgfx::getStats()->numDrawCallsPeak == 3);
    REQUIRE(renderer.cache_statistics().uploads == 1);
}
