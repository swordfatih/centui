#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <centui/platform.hpp>

#include <SFML/Graphics.hpp>

namespace centui::sfml
{

/// \brief Adapts an owned or borrowed SFML window to portable input events.
/// A borrowed window must outlive the adapter.
class platform final : public centui::platform
{
public:
    explicit platform(const window_options& options);

    explicit platform(::sf::RenderWindow& window);

    platform(const platform&) = delete;

    platform& operator=(const platform&) = delete;

    platform(platform&&) = delete;

    platform& operator=(platform&&) = delete;

    [[nodiscard]] ::sf::RenderWindow& native_window() const noexcept;

    void set_scale(float value);

    [[nodiscard]] centui::viewport viewport() const override;

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept override;

    [[nodiscard]] std::span<const input_event> events() const noexcept override;

    [[nodiscard]] bool poll_events() override;

    [[nodiscard]] bool poll_events(const std::function<void(const ::sf::Event&)>& on_event);

    [[nodiscard]] std::optional<input_event> translate(const ::sf::Event& event);

private:
    static std::unique_ptr<::sf::RenderWindow> create_window(const window_options& options);

    static std::shared_ptr<const host_services> make_services();

    static modifiers modifiers_now();

    static mouse_button translate_button(::sf::Mouse::Button button);

    static key translate_key(::sf::Keyboard::Key code);

    std::unique_ptr<::sf::RenderWindow> m_owned;
    ::sf::RenderWindow& m_window;

    std::shared_ptr<const host_services> m_services = make_services();

    float m_scale = 1;
    std::set<::sf::Keyboard::Scancode> m_pressed;
    std::vector<input_event> m_events;
};

} // namespace centui::sfml
