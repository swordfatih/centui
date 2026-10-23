#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <centui/platform.hpp>

#include <SDL3/SDL.h>

namespace centui::detail
{

// SDL video subsystem lifetime
class sdl_context final
{
public:
    sdl_context();

    ~sdl_context();

    sdl_context(const sdl_context&) = delete;

    sdl_context& operator=(const sdl_context&) = delete;

    sdl_context(sdl_context&&) = delete;

    sdl_context& operator=(sdl_context&&) = delete;
};

// SDL window ownership
struct window_deleter
{
    void operator()(SDL_Window* window) const noexcept;
};

using window_ptr = std::unique_ptr<SDL_Window, window_deleter>;

} // namespace centui::detail

namespace centui::sdl
{

/// \brief Adapts an owned or borrowed SDL window, including clipboard and text input.
/// A borrowed window must outlive the adapter.
class platform final : public centui::platform
{
public:
    explicit platform(const window_options& options);

    explicit platform(SDL_Window& window);

    ~platform() override;

    platform(const platform&) = delete;

    platform& operator=(const platform&) = delete;

    platform(platform&&) = delete;

    platform& operator=(platform&&) = delete;

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept override;

    [[nodiscard]] bool poll_events() override;

    [[nodiscard]] bool poll_events(const std::function<void(const SDL_Event&)>& on_event);

    [[nodiscard]] std::optional<input_event> translate(const SDL_Event& event);

    [[nodiscard]] std::span<const input_event> events() const noexcept override;

    [[nodiscard]] centui::viewport viewport() const override;

    [[nodiscard]] SDL_Window& native_window() const noexcept;

    [[nodiscard]] centui::native_window_handles native_handles() const;

private:
    static detail::window_ptr create_window(const window_options& options);

    void initialize_services();

    static modifiers translate_modifiers(SDL_Keymod value);

    static key translate_key(SDL_Keycode value);

    void release_mouse_capture() noexcept;

    [[nodiscard]] std::optional<std::pair<float, float>> to_logical_position(float x, float y) const;

    [[nodiscard]] static mouse_button translate_button(std::uint8_t button) noexcept;

    std::unique_ptr<detail::sdl_context> m_sdl;
    detail::window_ptr m_owned;
    SDL_Window& m_window;
    std::shared_ptr<std::reference_wrapper<SDL_Window>> m_lifetime =
        std::make_shared<std::reference_wrapper<SDL_Window>>(m_window);

    std::vector<input_event> m_input_events;
    std::shared_ptr<host_services> m_services;
    bool m_mouse_captured = false;
    std::uint8_t m_capture_button = 0;
};

} // namespace centui::sdl
