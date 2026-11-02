#include <centui/platforms/glfw.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("GLFW polling retains callbacks delivered before an adapter polls", "[platform][glfw]")
{
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    centui::glfw::platform first{{.title = "first"}};
    centui::glfw::platform second{{.title = "second"}};
    auto* window = &second.native_window();
    const auto callback = glfwSetKeyCallback(window, nullptr);
    REQUIRE(callback);
    glfwSetKeyCallback(window, callback);
    callback(window, GLFW_KEY_A, 0, GLFW_PRESS, 0);
    REQUIRE(first.poll_events());
    REQUIRE(second.poll_events());
    const auto first_events = second.take_events();
    REQUIRE(first_events.size() == 1);
    REQUIRE(std::get<centui::key_event>(first_events[0]).code == centui::key::a);
    REQUIRE(second.poll_events());
    REQUIRE(second.take_events().empty());
    callback(window, GLFW_KEY_C, 0, GLFW_PRESS, 0);
    const auto events = second.take_events();
    REQUIRE(events.size() == 1);
    REQUIRE(std::get<centui::key_event>(events[0]).code == centui::key::c);
    REQUIRE(second.take_events().empty());
}
