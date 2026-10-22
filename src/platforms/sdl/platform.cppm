module;

#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
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

#include <SDL3/SDL.h>

export module centui.sdl;

export import centui.platform;

namespace centui::detail
{

// SDL video subsystem lifetime
class sdl_context final
{
public:
    sdl_context()
    {
        if(!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            throw std::runtime_error(SDL_GetError());
        }
    }

    ~sdl_context()
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    sdl_context(const sdl_context&) = delete;
    sdl_context& operator=(const sdl_context&) = delete;

    sdl_context(sdl_context&&) = delete;
    sdl_context& operator=(sdl_context&&) = delete;
};

// SDL window ownership
struct window_deleter
{
    void operator()(SDL_Window* window) const noexcept
    {
        if(window)
        {
            SDL_DestroyWindow(window);
        }
    }
};

using window_ptr = std::unique_ptr<SDL_Window, window_deleter>;

} // namespace centui::detail

export namespace centui::sdl
{

struct native_window_handles
{
    void* window = nullptr;
    void* display = nullptr;
    bool wayland = false;
};

class platform final : public centui::platform
{
public:
    explicit platform(const window_options& options)
        : _sdl(std::make_unique<detail::sdl_context>()), _owned(create_window(options)), _window(*_owned)
    {
        initialize_services();
    }

    explicit platform(SDL_Window& window) : _window(window)
    {
        initialize_services();
    }

private:
    static detail::window_ptr create_window(const window_options& options)
    {
        if(options.width <= 0 || options.height <= 0)
        {
            throw std::invalid_argument("Window dimensions must be positive");
        }
        detail::window_ptr window{SDL_CreateWindow(options.title.c_str(), options.width, options.height,
                                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)};
        if(!window)
        {
            throw std::runtime_error(SDL_GetError());
        }
        return window;
    }

    void initialize_services()
    {
        _services = std::make_shared<host_services>();
        const std::weak_ptr<std::reference_wrapper<SDL_Window>> weak_window = _lifetime;
        _services->read_clipboard = [weak_window]
        {
            if(weak_window.expired())
            {
                return std::string{};
            }

            const auto text = std::shared_ptr<char>{SDL_GetClipboardText(), SDL_free};
            return text ? std::string{text.get()} : std::string{};
        };
        _services->write_clipboard = [weak_window](std::string_view text)
        {
            if(!weak_window.expired())
            {
                const std::string value{text};
                if(!SDL_SetClipboardText(value.c_str()))
                {
                    throw std::runtime_error(SDL_GetError());
                }
            }
        };
        _services->text_input = [weak_window](std::optional<rect> area)
        {
            const auto window = weak_window.lock();
            if(!window)
            {
                return;
            }

            if(!area)
            {
                SDL_StopTextInput(std::addressof(window->get()));
                return;
            }

            if(!SDL_TextInputActive(std::addressof(window->get())) && !SDL_StartTextInput(std::addressof(window->get())))
            {
                throw std::runtime_error(SDL_GetError());
            }

            int window_width = 0;
            int window_height = 0;
            int pixel_width = 0;
            int pixel_height = 0;
            SDL_GetWindowSize(std::addressof(window->get()), &window_width, &window_height);
            SDL_GetWindowSizeInPixels(std::addressof(window->get()), &pixel_width, &pixel_height);
            if(pixel_width > 0 && pixel_height > 0)
            {
                const auto scale = SDL_GetWindowDisplayScale(std::addressof(window->get()));
                const auto x_scale = scale * static_cast<float>(window_width) / pixel_width;
                const auto y_scale = scale * static_cast<float>(window_height) / pixel_height;
                const SDL_Rect input_area{static_cast<int>(std::floor(area->x * x_scale)),
                                          static_cast<int>(std::floor(area->y * y_scale)),
                                          std::max(1, static_cast<int>(std::ceil(area->width * x_scale))),
                                          std::max(1, static_cast<int>(std::ceil(area->height * y_scale)))};
                SDL_SetTextInputArea(std::addressof(window->get()), &input_area, 0);
            }
        };
    }

public:
    ~platform() override
    {
        release_mouse_capture();
    }

    platform(const platform&) = delete;
    platform& operator=(const platform&) = delete;

    platform(platform&&) = delete;
    platform& operator=(platform&&) = delete;

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept override
    {
        return _services;
    }

    [[nodiscard]] bool poll_events() override
    {
        return poll_events({});
    }

    [[nodiscard]] bool poll_events(const std::function<void(const SDL_Event&)>& on_event)
    {
        _input_events.clear();
        SDL_Event event;
        bool running = true;
        while(SDL_PollEvent(&event))
        {
            if(on_event)
            {
                on_event(event);
            }
            if(event.type == SDL_EVENT_QUIT ||
               (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(std::addressof(_window))))
            {
                running = false;
            }
            if(auto translated = translate(event))
            {
                _input_events.push_back(std::move(*translated));
            }
        }
        return running;
    }

    [[nodiscard]] std::optional<input_event> translate(const SDL_Event& event)
    {
        if(event.type == SDL_EVENT_QUIT)
        {
            return std::nullopt;
        }

        if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
           event.window.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            return std::nullopt;
        }

        if((event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED ||
            event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) &&
           event.window.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            if(event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
            {
                release_mouse_capture();
                return input_event{.type = event.type == SDL_EVENT_WINDOW_FOCUS_LOST
                                                     ? input_event_type::focus_lost
                                                     : input_event_type::pointer_cancelled};
            }
            if(event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
            {
                return input_event{.type = input_event_type::focus_gained};
            }
        }

        if(event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE && event.window.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            return input_event{.type = input_event_type::pointer_left};
        }

        if((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            return input_event{.type = event.type == SDL_EVENT_KEY_DOWN ? input_event_type::key_pressed : input_event_type::key_released,
                                     .code = translate_key(event.key.key), .mods = translate_modifiers(event.key.mod), .repeat = event.key.repeat};
        }

        if(event.type == SDL_EVENT_TEXT_INPUT && event.text.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            return input_event{.type = input_event_type::text_entered, .text = event.text.text ? event.text.text : ""};
        }

        if(event.type == SDL_EVENT_TEXT_EDITING && event.edit.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            const std::string text = event.edit.text ? event.edit.text : "";
            const auto byte_offset = [&](int characters)
            {
                std::size_t offset = 0;
                while(characters > 0 && offset < text.size())
                {
                    ++offset;
                    while(offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
                    {
                        ++offset;
                    }
                    --characters;
                }

                return offset;
            };
            const auto start = byte_offset(std::max(0, event.edit.start));
            const auto end = byte_offset(std::max(0, event.edit.start) + std::max(0, event.edit.length));
            return input_event{.type = input_event_type::composition, .text = text,
                                     .selection_start = start, .selection_length = end - start};
        }

        if(event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            if(const auto position = to_logical_position(event.wheel.mouse_x, event.wheel.mouse_y))
            {
                const auto direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -40.0f : 40.0f;
                return input_event{.type = input_event_type::wheel, .x = position->first, .y = position->second,
                                         .mods = translate_modifiers(SDL_GetModState()),
                                         .delta_x = event.wheel.x * direction, .delta_y = -event.wheel.y * direction};
            }
            return std::nullopt;
        }

        if(event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            if(const auto position = to_logical_position(event.motion.x, event.motion.y))
            {
                return input_event{.type = input_event_type::pointer_moved, .x = position->first, .y = position->second,
                                         .mods = translate_modifiers(SDL_GetModState())};
            }
            return std::nullopt;
        }

        if((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
           event.button.windowID == SDL_GetWindowID(std::addressof(_window)))
        {
            const auto type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? input_event_type::pointer_pressed : input_event_type::pointer_released;
            if(type == input_event_type::pointer_released && event.button.button == _capture_button)
            {
                release_mouse_capture();
            }
            if(const auto position = to_logical_position(event.button.x, event.button.y))
            {
                if(type == input_event_type::pointer_pressed && !_mouse_captured)
                {
                    _mouse_captured = SDL_CaptureMouse(true);
                    _capture_button = event.button.button;
                }
                return input_event{.type = type,
                                         .x = position->first,
                                         .y = position->second,
                                         .button = translate_button(event.button.button),
                                         .mods = translate_modifiers(SDL_GetModState())};
            }
            else
            {
                return input_event{.type = input_event_type::pointer_cancelled};
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::span<const input_event> events() const noexcept override
    {
        return _input_events;
    }

    [[nodiscard]] centui::viewport viewport() const override
    {
        int width = 0;
        int height = 0;
        if((SDL_GetWindowFlags(std::addressof(_window)) & SDL_WINDOW_MINIMIZED) ||
           !SDL_GetWindowSizeInPixels(std::addressof(_window), &width, &height))
        {
            return {};
        }
        return {width, height, SDL_GetWindowDisplayScale(std::addressof(_window))};
    }

    [[nodiscard]] SDL_Window& native_window() const noexcept
    {
        return _window;
    }

    [[nodiscard]] native_window_handles native_handles() const
    {
        const auto properties = SDL_GetWindowProperties(std::addressof(_window));
        if(properties == 0)
        {
            throw std::runtime_error(SDL_GetError());
        }

        for(const auto property: {SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                  SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,
                                  SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER})
        {
            if(const auto window = SDL_GetPointerProperty(properties, property, nullptr))
            {
                return {window};
            }
        }

        if(const auto window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr))
        {
            const auto display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            if(!display)
            {
                throw std::runtime_error("SDL did not expose the Wayland display");
            }
            return {window, display, true};
        }

        const auto window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if(window != 0)
        {
            const auto display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            if(!display)
            {
                throw std::runtime_error("SDL did not expose the X11 display");
            }
            return {reinterpret_cast<void*>(static_cast<std::uintptr_t>(window)), display};
        }

        throw std::runtime_error("SDL video driver has no supported native window properties");
    }

private:
    static modifiers translate_modifiers(SDL_Keymod value)
    {
        return {.shift = (value & SDL_KMOD_SHIFT) != 0, .control = (value & SDL_KMOD_CTRL) != 0,
                .alt = (value & SDL_KMOD_ALT) != 0, .system = (value & SDL_KMOD_GUI) != 0};
    }

    static key translate_key(SDL_Keycode value)
    {
        static constexpr std::array pairs{
            std::pair{SDLK_TAB, key::tab}, std::pair{SDLK_RETURN, key::enter}, std::pair{SDLK_KP_ENTER, key::enter},
            std::pair{SDLK_SPACE, key::space}, std::pair{SDLK_ESCAPE, key::escape},
            std::pair{SDLK_LEFT, key::left}, std::pair{SDLK_RIGHT, key::right},
            std::pair{SDLK_UP, key::up}, std::pair{SDLK_DOWN, key::down},
            std::pair{SDLK_HOME, key::home}, std::pair{SDLK_END, key::end},
            std::pair{SDLK_PAGEUP, key::page_up}, std::pair{SDLK_PAGEDOWN, key::page_down},
            std::pair{SDLK_BACKSPACE, key::backspace}, std::pair{SDLK_DELETE, key::delete_},
            std::pair{SDLK_A, key::a}, std::pair{SDLK_C, key::c}, std::pair{SDLK_V, key::v},
            std::pair{SDLK_X, key::x}, std::pair{SDLK_Y, key::y}, std::pair{SDLK_Z, key::z}};
        for(const auto& [native, translated]: pairs)
        {
            if(value == native)
            {
                return translated;
            }
        }

        return key::unknown;
    }

    void release_mouse_capture() noexcept
    {
        if(_mouse_captured)
        {
            SDL_CaptureMouse(false);
            _mouse_captured = false;
        }
    }

    [[nodiscard]] std::optional<std::pair<float, float>> to_logical_position(float x, float y) const
    {
        int window_width = 0;
        int window_height = 0;
        int pixel_width = 0;
        int pixel_height = 0;
        if((SDL_GetWindowFlags(std::addressof(_window)) & SDL_WINDOW_MINIMIZED) ||
           !SDL_GetWindowSize(std::addressof(_window), &window_width, &window_height) ||
           !SDL_GetWindowSizeInPixels(std::addressof(_window), &pixel_width, &pixel_height) ||
           window_width <= 0 || window_height <= 0 || pixel_width <= 0 || pixel_height <= 0)
        {
            return std::nullopt;
        }

        const auto scale = SDL_GetWindowDisplayScale(std::addressof(_window));
        return std::pair{
            x * static_cast<float>(pixel_width) / static_cast<float>(window_width) / scale,
            y * static_cast<float>(pixel_height) / static_cast<float>(window_height) / scale};
    }

    [[nodiscard]] static mouse_button translate_button(std::uint8_t button) noexcept
    {
        switch(button)
        {
        case SDL_BUTTON_LEFT:
        {
            return mouse_button::primary;
        }
        case SDL_BUTTON_RIGHT:
        {
            return mouse_button::secondary;
        }
        case SDL_BUTTON_MIDDLE:
        {
            return mouse_button::middle;
        }
        default:
        {
            return mouse_button::other;
        }
        }
    }

    std::unique_ptr<detail::sdl_context> _sdl;
    detail::window_ptr _owned;
    SDL_Window& _window;
    std::shared_ptr<std::reference_wrapper<SDL_Window>> _lifetime =
        std::make_shared<std::reference_wrapper<SDL_Window>>(_window);
    std::vector<input_event> _input_events;
    std::shared_ptr<host_services> _services;
    bool                     _mouse_captured = false;
    std::uint8_t             _capture_button = 0;
};

} // namespace centui::sdl
