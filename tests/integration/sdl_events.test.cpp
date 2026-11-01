#include <centui/platforms/sdl.hpp>

#include <catch2/catch_test_macros.hpp>

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
    REQUIRE(first.events().size() == 1);
    REQUIRE(first.events()[0].code == centui::key::c);
    REQUIRE(second.poll_events());
    REQUIRE(second.events().size() == 1);
    REQUIRE(second.events()[0].code == centui::key::a);
    REQUIRE(first.poll_events());
    REQUIRE(first.events().empty());
    REQUIRE(second.poll_events());
    REQUIRE(second.events().empty());

    event.type = SDL_EVENT_QUIT;
    REQUIRE(SDL_PushEvent(&event));
    REQUIRE_FALSE(first.poll_events());
    REQUIRE_FALSE(second.poll_events());
}
