#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct native_event
{
    centui::input_event value;
};

class test_platform final : public centui::platform
{
public:
    bool poll_events() override
    {
        return polls++ == 0;
    }

    std::span<const centui::input_event> events() const noexcept override
    {
        return pending;
    }

    centui::viewport viewport() const override
    {
        return {400, 200, 2};
    }

    std::shared_ptr<const centui::host_services> services() const noexcept override
    {
        return host;
    }

    std::optional<centui::input_event> translate(const native_event& event)
    {
        ++translations;

        return event.value;
    }

    int polls = 0;
    int translations = 0;
    std::vector<centui::input_event> pending;
    std::shared_ptr<centui::host_services> host = std::make_shared<centui::host_services>();
};

class test_renderer final : public centui::renderer
{
public:
    centui::viewport viewport() const override
    {
        return {1, 1};
    }

    void draw(const centui::draw_list& list) override
    {
        ++draws;
        surface = list.viewport;
    }

    bool begin_frame() override
    {
        ++begins;

        if(on_begin)
        {
            on_begin();
        }

        return ready;
    }

    void end_frame() override
    {
        ++ends;
    }

    int draws = 0;
    int begins = 0;
    int ends = 0;
    bool ready = true;
    std::function<void()> on_begin;
    centui::viewport surface;
};

class service_probe final : public centui::widget
{
public:
    using widget::request_frame;

    std::string clipboard() const
    {
        return host().read_clipboard ? host().read_clipboard() : std::string{};
    }

    int ticks = 0;
    centui::frame_duration elapsed{};

protected:
    void on_frame(centui::frame_duration duration) override
    {
        ++ticks;
        elapsed = duration;
    }
};

} // namespace

TEST_CASE("Native forwarding borrows host state without taking over its event loop", "[platform]")
{
    test_platform host;
    host.host->read_clipboard = []
    {
        return std::string{"attached"};
    };
    test_renderer renderer;
    centui::ui ui{host, renderer};
    auto probe = centui::make_widget<service_probe>();
    auto root = centui::container{probe};
    root->patch_style({.width = centui::percent(100)});
    centui::document document{root};

    ui.process_event(document, native_event{{.type = centui::input_event_type::pointer_moved, .x = 10, .y = 10}});
    ui.render(document);

    REQUIRE(host.translations == 1);
    REQUIRE(host.polls == 0);
    REQUIRE(renderer.begins == 0);
    REQUIRE(renderer.ends == 0);
    REQUIRE(renderer.draws == 1);
    REQUIRE(renderer.surface == host.viewport());
    REQUIRE(root->bounds().width == 200);
    REQUIRE(probe->clipboard() == "attached");
}

TEST_CASE("UI frames process polled events and advance widgets before drawing", "[platform][animation]")
{
    test_platform host;
    host.host->read_clipboard = []
    {
        return std::string{"attached"};
    };
    host.pending = {
        {.type = centui::input_event_type::pointer_pressed, .x = 5, .y = 5},
        {.type = centui::input_event_type::pointer_released, .x = 5, .y = 5},
    };

    test_renderer renderer;
    centui::ui ui{host, renderer};
    auto probe = centui::make_widget<service_probe>();
    bool clicked = false;
    auto action = centui::pressable{
        probe,
        {
            .on_click = [&]
            {
                clicked = true;
                probe->request_frame();
            },
        }
    };
    action.patch_style({.width = centui::px(100), .height = centui::px(60)});
    centui::document document{action};

    renderer.on_begin = [&]
    {
        REQUIRE(clicked);
        REQUIRE(probe->ticks == 1);
        REQUIRE(probe->elapsed == centui::frame_duration{0.25f});
        REQUIRE(probe->clipboard() == "attached");
    };

    REQUIRE(ui.frame(document, centui::frame_duration{0.25f}));

    REQUIRE(host.polls == 0);
    REQUIRE(renderer.begins == 1);
    REQUIRE(renderer.draws == 1);
    REQUIRE(renderer.ends == 1);
    REQUIRE(renderer.surface == host.viewport());
}

TEST_CASE("UI frames advance animations even when rendering is unavailable", "[platform][animation]")
{
    test_platform host;
    test_renderer renderer;
    renderer.ready = false;
    centui::platform& platform_interface = host;
    centui::renderer& renderer_interface = renderer;
    centui::ui ui{platform_interface, renderer_interface};
    auto probe = centui::make_widget<service_probe>();
    probe->request_frame();
    centui::document document{probe};

    REQUIRE_FALSE(ui.frame(document, centui::frame_duration{0.5f}));
    REQUIRE(probe->ticks == 1);
    REQUIRE(probe->elapsed == centui::frame_duration{0.5f});
    REQUIRE(host.polls == 0);
    REQUIRE(renderer.begins == 1);
    REQUIRE(renderer.draws == 0);
    REQUIRE(renderer.ends == 0);
}

TEST_CASE("UI frames support automatic timing and renderer-only applications", "[platform][animation]")
{
    test_renderer renderer;
    centui::ui ui{renderer};
    auto probe = centui::make_widget<service_probe>();
    centui::document document{probe};

    probe->request_frame();
    REQUIRE(ui.frame(document, centui::frame_duration{0.125f}));
    REQUIRE(probe->elapsed == centui::frame_duration{0.125f});

    probe->request_frame();
    REQUIRE(ui.frame(document));
    REQUIRE(probe->ticks == 2);
    REQUIRE(probe->elapsed.count() >= 0);
    REQUIRE(renderer.begins == 2);
    REQUIRE(renderer.draws == 2);
    REQUIRE(renderer.ends == 2);
    REQUIRE(renderer.surface == renderer.viewport());
}

TEST_CASE("UI frames reject invalid elapsed time before beginning rendering", "[platform]")
{
    test_renderer renderer;
    centui::ui ui{renderer};
    centui::document document{centui::container{}};

    REQUIRE_THROWS_AS(ui.frame(document, centui::frame_duration{-1}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        ui.frame(document, centui::frame_duration{std::numeric_limits<float>::infinity()}),
        std::invalid_argument
    );
    REQUIRE(renderer.begins == 0);
    REQUIRE(renderer.draws == 0);
    REQUIRE(renderer.ends == 0);
}
