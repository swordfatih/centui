#include <centui/backends/bgfx.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class surface_platform final : public centui::platform
{
public:
    bool poll_events() override
    {
        return true;
    }

    centui::viewport viewport() const override
    {
        return size;
    }

    std::shared_ptr<const centui::host_services> services() const noexcept override
    {
        return {};
    }

    std::optional<centui::render_surface> surface() const override
    {
        return target;
    }

    std::optional<centui::render_surface> target;
    centui::viewport size{64, 64};
};

} // namespace

TEST_CASE("bgfx rejects unavailable or invalid surfaces before graphics initialization", "[renderer][bgfx]")
{
    surface_platform platform;
    REQUIRE_THROWS_AS(centui::bgfx::backend{platform}, std::invalid_argument);

    platform.target = centui::native_surface{};
    REQUIRE_THROWS_AS(centui::bgfx::backend{platform}, std::invalid_argument);

    int handle = 0;
    platform.target = centui::native_surface{&handle, nullptr, centui::surface_handle_type::wayland};
    REQUIRE_THROWS_AS(centui::bgfx::backend{platform}, std::invalid_argument);

    platform.target = centui::named_surface{};
    REQUIRE_THROWS_AS(centui::bgfx::backend{platform}, std::invalid_argument);

    platform.target = centui::named_surface{std::string{"surface\0name", 12}};
    REQUIRE_THROWS_AS(centui::bgfx::backend{platform}, std::invalid_argument);

    platform.target = centui::native_surface{&handle};
    platform.size = {0, 64};
    {
        centui::bgfx::backend deferred{platform};
        REQUIRE_FALSE(deferred.begin_frame());
        platform.size = {64, 64};
        REQUIRE(platform.take_events().empty());
        // Reaching surface validation proves the deferred backend retries
        // initialization even without a queued metrics change.
        platform.target = centui::native_surface{};
        REQUIRE_THROWS_AS(deferred.begin_frame(), std::invalid_argument);
        REQUIRE_THROWS_AS(deferred.begin_frame(), std::invalid_argument);
    }
    platform.size = {65536, 64};
    REQUIRE_THROWS_AS(centui::bgfx::backend{platform}, std::invalid_argument);
}

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
