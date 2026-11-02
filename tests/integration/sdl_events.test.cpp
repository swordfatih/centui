#include <centui/platforms/sdl.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("SDL exposes a canvas selector as an owned named surface", "[platform][sdl]")
{
    centui::sdl::platform host{{.title = "surface"}};
    const auto properties = SDL_GetWindowProperties(&host.native_window());
    REQUIRE(properties != 0);
    REQUIRE(SDL_SetStringProperty(properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, "#application-canvas"));

    const auto surface = host.surface();
    REQUIRE(surface);
    REQUIRE(std::holds_alternative<centui::named_surface>(*surface));
    REQUIRE(std::get<centui::named_surface>(*surface).name == "#application-canvas");

    REQUIRE(SDL_ClearProperty(properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING));
    REQUIRE(std::get<centui::named_surface>(*surface).name == "#application-canvas");
}

TEST_CASE("Polling one SDL adapter preserves input for other windows", "[platform][sdl]")
{
    centui::sdl::platform first{{.title = "first"}};
    centui::sdl::platform second{{.title = "second"}};
    REQUIRE_THROWS_AS(centui::sdl::platform{second.native_window()}, std::logic_error);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(&second.native_window());
    event.key.key = SDLK_A;
    REQUIRE(SDL_PushEvent(&event));
    event.key.windowID = SDL_GetWindowID(&first.native_window());
    event.key.key = SDLK_C;
    REQUIRE(SDL_PushEvent(&event));

    REQUIRE(first.poll_events());
    const auto first_events = first.take_events();
    REQUIRE(first_events.size() == 1);
    REQUIRE(std::get<centui::key_event>(first_events[0]).code == centui::key::c);
    REQUIRE(second.poll_events());
    const auto second_events = second.take_events();
    REQUIRE(second_events.size() == 1);
    REQUIRE(std::get<centui::key_event>(second_events[0]).code == centui::key::a);
    REQUIRE(first.poll_events());
    REQUIRE(first.take_events().empty());
    REQUIRE(second.poll_events());
    REQUIRE(second.take_events().empty());

    event.type = SDL_EVENT_QUIT;
    REQUIRE(SDL_PushEvent(&event));
    REQUIRE_FALSE(first.poll_events());
    REQUIRE_FALSE(second.poll_events());
}

TEST_CASE("SDL translates touch and pen identities without duplicate emulated mouse events", "[platform][sdl]")
{
    centui::sdl::platform host{{.title = "touch"}};
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_Event event{};
    event.type = SDL_EVENT_FINGER_DOWN;
    event.tfinger.windowID = SDL_GetWindowID(&host.native_window());
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = 7;
    event.tfinger.x = 0.25f;
    event.tfinger.y = 0.5f;
    host.ingest(event);
    event.type = SDL_EVENT_FINGER_MOTION;
    event.tfinger.y = 0.6f;
    host.ingest(event);
    event.type = SDL_EVENT_FINGER_UP;
    host.ingest(event);
    const auto events = host.take_events();
    REQUIRE(events.size() == 3);
    const auto down = std::get<centui::pointer_event>(events[0]);
    REQUIRE(down.device == centui::pointer_device::touch);
    REQUIRE_FALSE(down.hover_capable);
    REQUIRE(down.pointer != 0);
    REQUIRE(down.x == Catch::Approx(host.viewport().logical_width() * 0.25f));
    REQUIRE(std::get<centui::pointer_event>(events[1]).pointer == down.pointer);
    REQUIRE(std::get<centui::pointer_event>(events[2]).pointer == down.pointer);

    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(&host.native_window());
    event.button.which = SDL_TOUCH_MOUSEID;
    host.ingest(event);
    REQUIRE(host.take_events().empty());

    event = {};
    event.type = SDL_EVENT_PEN_DOWN;
    event.ptouch.windowID = SDL_GetWindowID(&host.native_window());
    event.ptouch.which = 7;
    event.ptouch.x = 10;
    event.ptouch.y = 20;
    host.ingest(event);
    const auto pen_events = host.take_events();
    REQUIRE(pen_events.size() == 1);
    const auto pen = std::get<centui::pointer_event>(pen_events[0]);
    REQUIRE(pen.device == centui::pointer_device::pen);
    REQUIRE(pen.pointer != down.pointer);
}

TEST_CASE("SDL lifecycle watches queue activity changes for every adapter", "[platform][sdl]")
{
    centui::sdl::platform first{{.title = "first"}};
    centui::sdl::platform second{{.title = "second"}};
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_Event event{};
    event.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
    REQUIRE(SDL_PushEvent(&event));
    REQUIRE(first.poll_events());
    const auto events = second.take_events();
    REQUIRE(
        std::ranges::any_of(
            events,
            [](const auto& value)
            {
                const auto* lifecycle = std::get_if<centui::lifecycle_event>(&value);
                return lifecycle && lifecycle->activity == centui::application_activity::suspended;
            }
        )
    );
    REQUIRE_FALSE(second.state().active);
}
