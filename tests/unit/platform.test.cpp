#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
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

    bool begin_frame()
    {
        ++begins;

        return true;
    }

    void end_frame()
    {
        ++ends;
    }

    int draws = 0;
    int begins = 0;
    int ends = 0;
    centui::viewport surface;
};

class service_probe final : public centui::widget
{
public:
    std::string clipboard() const
    {
        return host().read_clipboard ? host().read_clipboard() : std::string{};
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

TEST_CASE("Convenience runner forwards events and owns frame sequencing", "[platform]")
{
    test_platform host;
    test_renderer renderer;
    auto root = centui::container{};
    centui::document document{root};

    centui::run(host, renderer, document);

    REQUIRE(host.polls == 2);
    REQUIRE(renderer.begins == 1);
    REQUIRE(renderer.draws == 1);
    REQUIRE(renderer.ends == 1);
    REQUIRE(renderer.surface == host.viewport());
}
