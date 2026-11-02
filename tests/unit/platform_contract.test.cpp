#include <thread>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class contract_platform final : public centui::platform
{
public:
    using platform::begin_request;
    using platform::enqueue;
    using platform::invalidate_resources;
    using platform::set_activity;
    using platform::set_keyboard_occlusion;
    using platform::set_safe_area;
    using platform::set_surface_available;

    bool poll_events() override
    {
        return true;
    }

    centui::viewport viewport() const override
    {
        return dimensions;
    }

    std::shared_ptr<const centui::host_services> services() const noexcept override
    {
        return host;
    }

    void synchronous_clipboard(std::function<std::string()> read, std::function<void(std::string_view)> write = {})
    {
        host = clipboard_services(std::move(read), std::move(write));
    }

    centui::viewport dimensions{200, 100};
    std::shared_ptr<centui::host_services> host = std::make_shared<centui::host_services>();
};

class contract_renderer final : public centui::renderer
{
public:
    centui::viewport viewport() const override
    {
        return {200, 100};
    }

    bool begin_frame() override
    {
        ++begins;
        return true;
    }

    void draw(const centui::draw_list&) override
    {
        ++draws;
    }

    void end_frame() override
    {
        ++ends;
    }

    void on_surface_event(const centui::surface_event& event) override
    {
        changes.push_back(event);
    }

    int begins = 0;
    int draws = 0;
    int ends = 0;
    std::vector<centui::surface_event> changes;
};

class animation_probe final : public centui::widget
{
public:
    using widget::request_frame;
    std::vector<centui::frame_duration> elapsed;

protected:
    void on_frame(centui::frame_duration value) override
    {
        elapsed.push_back(value);
        request_frame();
    }
};

void use_font(centui::document& document)
{
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
}

centui::input_event touch(centui::input_event_type type, centui::pointer_id id, float x, float y)
{
    return {
        .type = type,
        .x = x,
        .y = y,
        .pointer = id,
        .device = centui::pointer_device::touch,
        .hover_capable = false
    };
}

} // namespace

TEST_CASE("Platform queues drain once and service callbacks only enqueue results", "[platform][contract]")
{
    std::function<void(centui::service_status, std::string)> late;
    {
        contract_platform host;
        const auto pending = host.begin_request();
        late = pending.complete;
        host.enqueue(centui::input_event{.type = centui::input_event_type::key_pressed, .code = centui::key::a});
        pending.complete(centui::service_status::success, "font-independent data");
        pending.complete(centui::service_status::failed, "duplicate");
        const auto events = host.take_events();
        REQUIRE(events.size() == 2);
        REQUIRE(std::get<centui::key_event>(events[0]).code == centui::key::a);
        REQUIRE(std::get<centui::service_result>(events[1]).request == pending.id);
        REQUIRE(std::get<centui::service_result>(events[1]).status == centui::service_status::success);
        REQUIRE(host.take_events().empty());
        const auto threaded = host.begin_request();
        std::thread worker{[complete = threaded.complete]
                           {
                               complete(centui::service_status::success, "from another thread");
                           }};
        worker.join();
        REQUIRE(std::get<centui::service_result>(host.take_events()[0]).request == threaded.id);
        late = host.begin_request().complete;
    }
    REQUIRE_NOTHROW(late(centui::service_status::success, "after platform destruction"));
}

TEST_CASE("Synchronous clipboard adapters use the same result queue and report errors", "[platform][contract]")
{
    contract_platform host;
    host.synchronous_clipboard(
        []
        {
            return std::string{"Inter"};
        }
    );
    const auto id = host.services()->read_clipboard();
    const auto result = host.take_events();
    REQUIRE(result.size() == 1);
    REQUIRE(std::get<centui::service_result>(result[0]).request == id);
    REQUIRE(std::get<centui::service_result>(result[0]).text == "Inter");
    const auto unsupported = host.services()->write_clipboard("text");
    REQUIRE(std::get<centui::service_result>(host.take_events()[0]).status == centui::service_status::unsupported);
    REQUIRE(unsupported != id);
    host.synchronous_clipboard(
        []() -> std::string
        {
            throw std::runtime_error("unavailable");
        }
    );
    REQUIRE_NOTHROW(host.services()->read_clipboard());
    REQUIRE(std::get<centui::service_result>(host.take_events()[0]).status == centui::service_status::failed);
}

TEST_CASE(
    "Clipboard completion targets its original input and rejects stale selection or focus",
    "[platform][contract][input]"
)
{
    contract_platform host;
    contract_renderer renderer;
    centui::ui ui{host, renderer};
    auto first = centui::input{"first"};
    auto second = centui::input{"second"};
    auto root = centui::container{first, second};
    centui::document document{root, {.system_font_discovery = false}};
    use_font(document);
    std::function<void(centui::service_status, std::string)> complete;
    host.host->read_clipboard = [&]
    {
        auto request = host.begin_request();
        complete = request.complete;
        return request.id;
    };
    REQUIRE(ui.frame(document, centui::frame_duration::zero()));
    REQUIRE(first->request_focus());
    first->set_selection(0, 5);
    const auto paste = [&]
    {
        ui.process_event(
            document,
            centui::input_event{
                .type = centui::input_event_type::key_pressed,
                .code = centui::key::v,
                .mods = {.control = true}
            }
        );
    };

    SECTION("Completion waits until the queue is consumed")
    {
        paste();
        complete(centui::service_status::success, "pasted");
        REQUIRE(first->value() == "first");
        REQUIRE(ui.frame(document, centui::frame_duration::zero()));
        REQUIRE(first->value() == "pasted");
        REQUIRE(second->value() == "second");
    }
    SECTION("A changed selection invalidates the request")
    {
        paste();
        first->set_selection(0, 0);
        complete(centui::service_status::success, "stale");
        REQUIRE(ui.frame(document, centui::frame_duration::zero()));
        REQUIRE(first->value() == "first");
    }
    SECTION("Moving focus away and back invalidates the request")
    {
        paste();
        REQUIRE(second->request_focus());
        REQUIRE(first->request_focus());
        complete(centui::service_status::success, "stale");
        REQUIRE(ui.frame(document, centui::frame_duration::zero()));
        REQUIRE(first->value() == "first");
        REQUIRE(second->value() == "second");
    }
    SECTION("Removing the input invalidates its request")
    {
        paste();
        root->remove(first);
        complete(centui::service_status::success, "stale");
        REQUIRE(ui.frame(document, centui::frame_duration::zero()));
        REQUIRE(first->value() == "first");
    }
    SECTION("A failed cut retains the selected text")
    {
        host.host->write_clipboard = [&](std::string)
        {
            auto request = host.begin_request();
            complete = request.complete;
            return request.id;
        };
        ui.process_event(
            document,
            centui::input_event{
                .type = centui::input_event_type::key_pressed,
                .code = centui::key::x,
                .mods = {.control = true}
            }
        );
        REQUIRE(first->value() == "first");
        complete(centui::service_status::denied, {});
        REQUIRE(ui.frame(document, centui::frame_duration::zero()));
        REQUIRE(first->value() == "first");
    }
}

TEST_CASE("Independent touch pointers keep their captures without creating hover", "[platform][contract][touch]")
{
    int first_clicks = 0;
    int second_clicks = 0;
    auto first = centui::button{
        "first",
        {.on_click = [&]
         {
             ++first_clicks;
         }}
    };
    auto second = centui::button{
        "second",
        {.on_click = [&]
         {
             ++second_clicks;
         }}
    };
    first->patch_style({.width = centui::px(100), .height = centui::px(50)});
    second->patch_style({.width = centui::px(100), .height = centui::px(50)});
    auto root = centui::container{first, second};
    root->patch_style({.width = centui::px(200), .flex_direction = centui::direction::row});
    centui::document document{root, {.system_font_discovery = false}};
    use_font(document);
    const centui::viewport viewport{200, 100};
    document.process_event(touch(centui::input_event_type::pointer_pressed, 1, 10, 10), viewport);
    document.process_event(touch(centui::input_event_type::pointer_pressed, 2, 110, 10), viewport);
    REQUIRE(first->pressed());
    REQUIRE(second->pressed());
    REQUIRE_FALSE(first->hovered());
    REQUIRE_FALSE(second->hovered());
    SECTION("Losing activity cancels every touch capture")
    {
        document.process_event({.type = centui::input_event_type::focus_lost}, viewport);
        REQUIRE_FALSE(first->pressed());
        REQUIRE_FALSE(second->pressed());
        document.process_event({.type = centui::input_event_type::focus_gained}, viewport);
        document.process_event(touch(centui::input_event_type::pointer_released, 1, 10, 10), viewport);
        document.process_event(touch(centui::input_event_type::pointer_released, 2, 110, 10), viewport);
        REQUIRE(first_clicks == 0);
        REQUIRE(second_clicks == 0);
    }
    SECTION("Releasing one touch preserves another capture")
    {
        document.process_event(touch(centui::input_event_type::pointer_released, 1, 10, 10), viewport);
        REQUIRE(first_clicks == 1);
        REQUIRE(second_clicks == 0);
        REQUIRE(second->pressed());
        document.process_event(touch(centui::input_event_type::pointer_released, 2, 110, 10), viewport);
        REQUIRE(second_clicks == 1);
        REQUIRE_FALSE(second->pressed());
    }
}

TEST_CASE("Touch scrolling cancels a child press while a tap still activates it", "[platform][contract][touch][scroll]")
{
    int clicks = 0;
    auto button = centui::button{
        "tap",
        {.on_click = [&]
         {
             ++clicks;
         }}
    };
    button->patch_style({.height = centui::px(100), .flex = centui::flex::all(0, 0, centui::px(100))});
    auto spacer = centui::container{};
    spacer->patch_style({.height = centui::px(200), .flex = centui::flex::all(0, 0, centui::px(200))});
    auto scroll = centui::scroll{button, spacer};
    scroll->patch_style({.width = centui::px(100), .height = centui::px(100)});
    centui::document document{scroll, {.system_font_discovery = false}};
    use_font(document);
    const centui::viewport viewport{100, 100};
    document.process_event(touch(centui::input_event_type::pointer_pressed, 1, 20, 50), viewport);
    document.process_event(touch(centui::input_event_type::pointer_released, 1, 20, 50), viewport);
    REQUIRE(clicks == 1);
    document.process_event(touch(centui::input_event_type::pointer_pressed, 2, 20, 70), viewport);
    document.process_event(touch(centui::input_event_type::pointer_moved, 2, 20, 40), viewport);
    REQUIRE(scroll->offset().y == 30);
    REQUIRE_FALSE(button->pressed());
    document.process_event(touch(centui::input_event_type::pointer_released, 2, 20, 40), viewport);
    REQUIRE(clicks == 1);
}

TEST_CASE(
    "Lifecycle and surface events gate drawing and reset animation timing on resume",
    "[platform][contract][lifecycle]"
)
{
    contract_platform host;
    contract_renderer renderer;
    centui::ui ui{host, renderer};
    auto probe = centui::make_widget<animation_probe>();
    probe->request_frame();
    centui::document document{probe, {.system_font_discovery = false}};
    REQUIRE(ui.frame(document, centui::frame_duration{0.1f}));
    host.enqueue(centui::lifecycle_event{centui::application_activity::suspended});
    host.set_safe_area({1, 0, 0, 0});
    REQUIRE_FALSE(ui.frame(document, centui::frame_duration{10}));
    REQUIRE(probe->elapsed.size() == 1);
    REQUIRE(renderer.draws == 1);
    REQUIRE(
        std::ranges::all_of(
            renderer.changes,
            [](const auto& event)
            {
                return !event.state.active;
            }
        )
    );
    host.set_activity(centui::application_activity::active);
    REQUIRE(ui.frame(document, centui::frame_duration{10}));
    REQUIRE(probe->elapsed.back() == centui::frame_duration::zero());
    host.set_surface_available(false);
    REQUIRE_FALSE(ui.frame(document, centui::frame_duration{0.1f}));
    REQUIRE(renderer.changes.back().change == centui::surface_change::metrics);
    const auto generation = host.state().surface_generation;
    host.set_surface_available(true);
    REQUIRE(ui.frame(document, centui::frame_duration{0.1f}));
    REQUIRE(host.state().surface_generation == generation + 1);
    host.set_safe_area({1, 2, 3, 4});
    host.set_keyboard_occlusion(centui::rect{0, 60, 200, 40});
    REQUIRE(ui.frame(document, centui::frame_duration{0.1f}));
    REQUIRE(host.state().safe_area.bottom == 3);
    REQUIRE(renderer.changes.back().state.keyboard_occlusion->y == 60);
}
