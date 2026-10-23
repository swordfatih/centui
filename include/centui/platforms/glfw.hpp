#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <centui/platform.hpp>

#include <GLFW/glfw3.h>

namespace centui::glfw::detail
{

struct context
{
    context();

    ~context();
};

std::shared_ptr<context> acquire_context();

struct window_deleter
{
    void operator()(GLFWwindow* window) const noexcept;
};

} // namespace centui::glfw::detail

namespace centui::glfw
{

/// \brief Adapts an owned or borrowed GLFW window and chains its existing callbacks.
/// A borrowed window must outlive the adapter.
class platform final : public centui::platform
{
public:
    explicit platform(const window_options& options);

    explicit platform(GLFWwindow& window);

    ~platform() override;

    platform(const platform&) = delete;

    platform& operator=(const platform&) = delete;

    platform(platform&&) = delete;

    platform& operator=(platform&&) = delete;

    [[nodiscard]] bool poll_events() override;

    [[nodiscard]] std::span<const input_event> events() const noexcept override;

    // Use this after an application-owned glfwPollEvents() call.
    [[nodiscard]] std::vector<input_event> take_events();

    [[nodiscard]] centui::viewport viewport() const override;

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept override;

    [[nodiscard]] GLFWwindow& native_window() const noexcept;

    [[nodiscard]] centui::native_window_handles native_handles() const;

private:
    static std::unique_ptr<GLFWwindow, detail::window_deleter> create_window(const window_options& options);

    void initialize();

    template <typename Setter, typename Callback>
    void restore(Setter setter, Callback previous, Callback installed) noexcept
    {
        const auto current = setter(&m_window, previous);

        if(current != installed)
        {
            setter(&m_window, current);
        }
    }

    template <typename Callback>
    static void dispatch(GLFWwindow* window, Callback&& callback) noexcept
    {
        const auto found = m_adapters.find(window);

        if(found == m_adapters.end())
        {
            return;
        }

        auto& self = found->second.get();

        try
        {
            callback(self);
        }
        catch(...)
        {
            self.m_callback_error = std::current_exception();
        }
    }

    void check_callback_error();

    [[nodiscard]] modifiers modifiers_now() const noexcept;

    [[nodiscard]] static key translate_key(int value) noexcept;

    [[nodiscard]] std::pair<float, float> pointer(double x, double y) const;

    static void key_callback(GLFWwindow* window, int native, int scancode, int action, int mods);

    static void char_callback(GLFWwindow* window, unsigned int codepoint);

    static void cursor_callback(GLFWwindow* window, double x, double y);

    static void enter_callback(GLFWwindow* window, int entered);

    static void button_callback(GLFWwindow* window, int native, int action, int mods);

    static void scroll_callback(GLFWwindow* window, double horizontal, double vertical);

    static void focus_callback(GLFWwindow* window, int focused);

    std::shared_ptr<detail::context> m_context;
    std::unique_ptr<GLFWwindow, detail::window_deleter> m_owned;
    GLFWwindow& m_window;

    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);

    std::shared_ptr<host_services> m_services;
    std::vector<input_event> m_events;
    std::exception_ptr m_callback_error;
    GLFWkeyfun m_previous_key = nullptr;
    GLFWcharfun m_previous_char = nullptr;
    GLFWcursorposfun m_previous_cursor = nullptr;
    GLFWcursorenterfun m_previous_enter = nullptr;
    GLFWmousebuttonfun m_previous_button = nullptr;
    GLFWscrollfun m_previous_scroll = nullptr;
    GLFWwindowfocusfun m_previous_focus = nullptr;
    static inline std::unordered_map<GLFWwindow*, std::reference_wrapper<platform>> m_adapters;
};

} // namespace centui::glfw
