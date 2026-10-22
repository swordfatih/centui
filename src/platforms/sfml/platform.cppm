module;

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

#include <SFML/Graphics.hpp>

export module centui.sfml.platform;
export import centui.platform;

export namespace centui::sfml
{
class platform final : public centui::platform
{
public:
    explicit platform(const window_options& options)
        : _owned(create_window(options)), _window(*_owned)
    {
    }

    explicit platform(::sf::RenderWindow& window) : _window(window)
    {
    }

    platform(const platform&) = delete;
    platform& operator=(const platform&) = delete;
    platform(platform&&) = delete;
    platform& operator=(platform&&) = delete;

    [[nodiscard]] ::sf::RenderWindow& native_window() const noexcept
    {
        return _window;
    }

    void set_scale(float value)
    {
        if(!std::isfinite(value) || value <= 0)
        {
            throw std::invalid_argument("Display scale must be finite and positive");
        }
        _scale = value;
    }

    [[nodiscard]] centui::viewport viewport() const override
    {
        if(!_window.isOpen())
        {
            return {};
        }
        const auto size = _window.getSize();
        const auto maximum = static_cast<unsigned int>(std::numeric_limits<int>::max());
        return {static_cast<int>(std::min(size.x, maximum)), static_cast<int>(std::min(size.y, maximum)), _scale};
    }

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept override
    {
        return _services;
    }

    [[nodiscard]] std::span<const input_event> events() const noexcept override
    {
        return _events;
    }

    [[nodiscard]] bool poll_events() override
    {
        return poll_events({});
    }

    [[nodiscard]] bool poll_events(const std::function<void(const ::sf::Event&)>& on_event)
    {
        _events.clear();
        while(const auto event = _window.pollEvent())
        {
            if(on_event)
            {
                on_event(*event);
            }
            if(auto translated = translate(*event))
            {
                _events.push_back(std::move(*translated));
            }
            if(event->is<::sf::Event::Closed>())
            {
                _window.close();
            }
            if(!_window.isOpen())
            {
                break;
            }
        }
        return _window.isOpen();
    }

    [[nodiscard]] std::optional<input_event> translate(const ::sf::Event& event)
    {
        const auto scale = viewport().scale;
        return event.visit([&](const auto& native) -> std::optional<input_event>
        {
            using Event = std::decay_t<decltype(native)>;
            if constexpr(std::is_same_v<Event, ::sf::Event::KeyPressed> || std::is_same_v<Event, ::sf::Event::KeyReleased>)
            {
                constexpr bool pressed = std::is_same_v<Event, ::sf::Event::KeyPressed>;
                const bool repeat = pressed && !_pressed.insert(native.scancode).second;
                if constexpr(!pressed)
                {
                    _pressed.erase(native.scancode);
                }
                return input_event{.type = pressed ? input_event_type::key_pressed : input_event_type::key_released,
                                   .code = translate_key(native.code),
                                   .mods = {native.shift, native.control, native.alt, native.system}, .repeat = repeat};
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::TextEntered>)
            {
                if(native.unicode >= 32 && native.unicode != 127 && native.unicode <= 0x10FFFF &&
                   !(native.unicode >= 0xD800 && native.unicode <= 0xDFFF))
                {
                    const auto utf8 = ::sf::String{native.unicode}.toUtf8();
                    return input_event{.type = input_event_type::text_entered, .text = std::string{utf8.begin(), utf8.end()}};
                }
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::MouseMoved>)
            {
                return input_event{.type = input_event_type::pointer_moved,
                                   .x = native.position.x / scale, .y = native.position.y / scale, .mods = modifiers_now()};
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::MouseButtonPressed> || std::is_same_v<Event, ::sf::Event::MouseButtonReleased>)
            {
                const auto button = translate_button(native.button);
                if(button != mouse_button::other)
                {
                    return input_event{.type = std::is_same_v<Event, ::sf::Event::MouseButtonPressed> ?
                                              input_event_type::pointer_pressed : input_event_type::pointer_released,
                                       .x = native.position.x / scale, .y = native.position.y / scale,
                                       .button = button, .mods = modifiers_now()};
                }
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::MouseWheelScrolled>)
            {
                const bool vertical = native.wheel == ::sf::Mouse::Wheel::Vertical;
                return input_event{.type = input_event_type::wheel,
                                   .x = native.position.x / scale, .y = native.position.y / scale, .mods = modifiers_now(),
                                   .delta_x = vertical ? 0 : -native.delta * 40,
                                   .delta_y = vertical ? -native.delta * 40 : 0};
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::MouseLeft>)
            {
                return input_event{.type = input_event_type::pointer_left};
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::FocusLost> || std::is_same_v<Event, ::sf::Event::Closed>)
            {
                _pressed.clear();
                return input_event{.type = input_event_type::focus_lost};
            }
            else if constexpr(std::is_same_v<Event, ::sf::Event::FocusGained>)
            {
                return input_event{.type = input_event_type::focus_gained};
            }
            return std::nullopt;
        });
    }

private:
    static std::unique_ptr<::sf::RenderWindow> create_window(const window_options& options)
    {
        if(options.width <= 0 || options.height <= 0)
        {
            throw std::invalid_argument("Window dimensions must be positive");
        }
        auto window = std::make_unique<::sf::RenderWindow>(
            ::sf::VideoMode{{static_cast<unsigned int>(options.width), static_cast<unsigned int>(options.height)}}, options.title);
        window->setVerticalSyncEnabled(true);
        return window;
    }

    static std::shared_ptr<const host_services> make_services()
    {
        auto services = std::make_shared<host_services>();
        services->read_clipboard = []
        {
            const auto utf8 = ::sf::Clipboard::getString().toUtf8();
            return std::string{utf8.begin(), utf8.end()};
        };
        services->write_clipboard = [](std::string_view value)
        {
            ::sf::Clipboard::setString(::sf::String::fromUtf8(value.begin(), value.end()));
        };
        services->text_input = [](std::optional<rect> area)
        {
            ::sf::Keyboard::setVirtualKeyboardVisible(area.has_value());
        };
        return services;
    }

    static modifiers modifiers_now()
    {
        using K = ::sf::Keyboard::Key;
        const auto down = [](K key)
        {
            return ::sf::Keyboard::isKeyPressed(key);
        };
        return {down(K::LShift) || down(K::RShift), down(K::LControl) || down(K::RControl),
                down(K::LAlt) || down(K::RAlt), down(K::LSystem) || down(K::RSystem)};
    }

    static mouse_button translate_button(::sf::Mouse::Button button)
    {
        switch(button)
        {
        case ::sf::Mouse::Button::Left:
        {
            return mouse_button::primary;
        }
        case ::sf::Mouse::Button::Right:
        {
            return mouse_button::secondary;
        }
        case ::sf::Mouse::Button::Middle:
        {
            return mouse_button::middle;
        }
        default:
        {
            return mouse_button::other;
        }
        }
    }

    static key translate_key(::sf::Keyboard::Key code)
    {
        using K = ::sf::Keyboard::Key;
        constexpr std::array mapping{
            std::pair{K::Tab, key::tab}, std::pair{K::Enter, key::enter}, std::pair{K::Space, key::space},
            std::pair{K::Escape, key::escape}, std::pair{K::Left, key::left}, std::pair{K::Right, key::right},
            std::pair{K::Up, key::up}, std::pair{K::Down, key::down}, std::pair{K::Home, key::home},
            std::pair{K::End, key::end}, std::pair{K::PageUp, key::page_up}, std::pair{K::PageDown, key::page_down},
            std::pair{K::Backspace, key::backspace}, std::pair{K::Delete, key::delete_},
            std::pair{K::A, key::a}, std::pair{K::C, key::c}, std::pair{K::V, key::v},
            std::pair{K::X, key::x}, std::pair{K::Y, key::y}, std::pair{K::Z, key::z}};
        for(const auto& [native, portable]: mapping)
        {
            if(code == native)
            {
                return portable;
            }
        }
        return key::unknown;
    }

    std::unique_ptr<::sf::RenderWindow> _owned;
    ::sf::RenderWindow& _window;
    std::shared_ptr<const host_services> _services = make_services();
    float _scale = 1;
    std::set<::sf::Keyboard::Scancode> _pressed;
    std::vector<input_event> _events;
};
}
