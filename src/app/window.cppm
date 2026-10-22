
module;

#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

export module centui.window;

export import centui.types;

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

using window_ptr = std::shared_ptr<SDL_Window>;

// bgfx lifetime
class bgfx_context final
{
public:
    explicit bgfx_context(const bgfx::Init& init)
    {
        if(!bgfx::init(init))
        {
            throw std::runtime_error("Failed to initialize bgfx");
        }
    }

    ~bgfx_context()
    {
        bgfx::shutdown();
    }

    bgfx_context(const bgfx_context&) = delete;
    bgfx_context& operator=(const bgfx_context&) = delete;

    bgfx_context(bgfx_context&&) = delete;
    bgfx_context& operator=(bgfx_context&&) = delete;
};

} // namespace centui::detail

export namespace centui
{

class window final
{
public:
    window(const std::string& title, int width, int height)
    {
        _window = detail::window_ptr{SDL_CreateWindow(
            title.c_str(),
            width,
            height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY), detail::window_deleter{}};

        if(!_window)
        {
            throw std::runtime_error(SDL_GetError());
        }

        _services = std::make_shared<host_services>();
        const std::weak_ptr<SDL_Window> weak_window = _window;
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
                SDL_StopTextInput(window.get());
                return;
            }

            if(!SDL_TextInputActive(window.get()) && !SDL_StartTextInput(window.get()))
            {
                throw std::runtime_error(SDL_GetError());
            }

            int window_width = 0;
            int window_height = 0;
            int pixel_width = 0;
            int pixel_height = 0;
            SDL_GetWindowSize(window.get(), &window_width, &window_height);
            SDL_GetWindowSizeInPixels(window.get(), &pixel_width, &pixel_height);
            if(pixel_width > 0 && pixel_height > 0)
            {
                const auto scale = SDL_GetWindowDisplayScale(window.get());
                const auto x_scale = scale * static_cast<float>(window_width) / pixel_width;
                const auto y_scale = scale * static_cast<float>(window_height) / pixel_height;
                const SDL_Rect input_area{static_cast<int>(std::floor(area->x * x_scale)),
                                          static_cast<int>(std::floor(area->y * y_scale)),
                                          std::max(1, static_cast<int>(std::ceil(area->width * x_scale))),
                                          std::max(1, static_cast<int>(std::ceil(area->height * y_scale)))};
                SDL_SetTextInputArea(window.get(), &input_area, 0);
            }
        };

#if defined(_WIN32)
        void* native_window = SDL_GetPointerProperty(
            SDL_GetWindowProperties(_window.get()),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr);
#else
#error "Native window adapter not yet implemented"
#endif

        if(!native_window)
        {
            throw std::runtime_error("Failed to obtain native window handle");
        }

        if(!SDL_GetWindowSizeInPixels(_window.get(), &_viewport.width, &_viewport.height))
        {
            throw std::runtime_error(SDL_GetError());
        }

        if(_viewport.width <= 0 || _viewport.height <= 0)
        {
            throw std::runtime_error("Invalid window pixel dimensions");
        }

        bgfx::Init init;
        init.swapChain.nwh = native_window;
        init.type = bgfx::RendererType::Direct3D11;
        init.swapChain.width = static_cast<std::uint32_t>(_viewport.width);
        init.swapChain.height = static_cast<std::uint32_t>(_viewport.height);
        init.reset = BGFX_RESET_VSYNC;

        _bgfx = std::make_unique<detail::bgfx_context>(init);
        _viewport.scale = SDL_GetWindowDisplayScale(_window.get());
        _backbuffer = _viewport;

        bgfx::setViewClear(
            0,
            BGFX_CLEAR_COLOR,
            0x202840FF);
    }

    ~window()
    {
        release_mouse_capture();
    }

    window(const window&) = delete;
    window& operator=(const window&) = delete;

    window(window&&) = delete;
    window& operator=(window&&) = delete;

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept
    {
        return _services;
    }

    [[nodiscard]] bool poll_events()
    {
        _input_events.clear();
        SDL_Event event;
        bool      running = true;

        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_EVENT_QUIT)
            {
                running = false;
                continue;
            }

            if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
               event.window.windowID == SDL_GetWindowID(_window.get()))
            {
                running = false;
                continue;
            }

            if((event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED ||
                event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) &&
               event.window.windowID == SDL_GetWindowID(_window.get()))
            {
                if(event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    release_mouse_capture();
                    _input_events.push_back({.type = event.type == SDL_EVENT_WINDOW_FOCUS_LOST
                                                         ? input_event_type::focus_lost
                                                         : input_event_type::pointer_cancelled});
                    continue;
                }
                if(event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
                {
                    _input_events.push_back({.type = input_event_type::focus_gained});
                    continue;
                }
            }

            if(event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE && event.window.windowID == SDL_GetWindowID(_window.get()))
            {
                _input_events.push_back({.type = input_event_type::pointer_left});
                continue;
            }

            if((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == SDL_GetWindowID(_window.get()))
            {
                _input_events.push_back({.type = event.type == SDL_EVENT_KEY_DOWN ? input_event_type::key_pressed : input_event_type::key_released,
                                         .code = translate_key(event.key.key), .mods = translate_modifiers(event.key.mod), .repeat = event.key.repeat});
                continue;
            }

            if(event.type == SDL_EVENT_TEXT_INPUT && event.text.windowID == SDL_GetWindowID(_window.get()))
            {
                _input_events.push_back({.type = input_event_type::text_entered, .text = event.text.text ? event.text.text : ""});
                continue;
            }

            if(event.type == SDL_EVENT_TEXT_EDITING && event.edit.windowID == SDL_GetWindowID(_window.get()))
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
                _input_events.push_back({.type = input_event_type::composition, .text = text,
                                         .selection_start = start, .selection_length = end - start});
                continue;
            }

            if(event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.windowID == SDL_GetWindowID(_window.get()))
            {
                if(const auto position = to_logical_position(event.wheel.mouse_x, event.wheel.mouse_y))
                {
                    const auto direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -40.0f : 40.0f;
                    _input_events.push_back({.type = input_event_type::wheel, .x = position->first, .y = position->second,
                                             .mods = translate_modifiers(SDL_GetModState()),
                                             .delta_x = event.wheel.x * direction, .delta_y = -event.wheel.y * direction});
                }
                continue;
            }

            if(event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == SDL_GetWindowID(_window.get()))
            {
                if(const auto position = to_logical_position(event.motion.x, event.motion.y))
                {
                    _input_events.push_back({.type = input_event_type::pointer_moved, .x = position->first, .y = position->second,
                                             .mods = translate_modifiers(SDL_GetModState())});
                }
                continue;
            }

            if((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
               event.button.windowID == SDL_GetWindowID(_window.get()))
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
                    _input_events.push_back({.type = type,
                                             .x = position->first,
                                             .y = position->second,
                                             .button = translate_button(event.button.button),
                                             .mods = translate_modifiers(SDL_GetModState())});
                }
                else
                {
                    _input_events.push_back({.type = input_event_type::pointer_cancelled});
                }
            }
        }

        refresh_viewport();
        return running;
    }

    [[nodiscard]] std::vector<input_event> take_input_events()
    {
        return std::exchange(_input_events, {});
    }

    // The backend borrows this viewport. Refresh it before input, independently
    // of whether a graphics frame will be submitted.
    void refresh_viewport()
    {
        int width = 0;
        int height = 0;
        if((SDL_GetWindowFlags(_window.get()) & SDL_WINDOW_MINIMIZED) ||
           !SDL_GetWindowSizeInPixels(_window.get(), &width, &height) || width <= 0 || height <= 0)
        {
            _viewport = {};
            return;
        }
        _viewport = {width, height, SDL_GetWindowDisplayScale(_window.get())};
    }

    [[nodiscard]] bool begin_frame()
    {
        refresh_viewport();
        if(_viewport.width <= 0 || _viewport.height <= 0)
        {
            return false;
        }
        if(_viewport.width > std::numeric_limits<std::uint16_t>::max() ||
           _viewport.height > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::invalid_argument("Window exceeds bgfx view dimensions");
        }

        if(_viewport.width != _backbuffer.width || _viewport.height != _backbuffer.height)
        {
            bgfx::SwapChain swap_chain;
            swap_chain.width = static_cast<std::uint32_t>(_viewport.width);
            swap_chain.height = static_cast<std::uint32_t>(_viewport.height);

            bgfx::reset(BGFX_RESET_VSYNC, &swap_chain);
            _backbuffer = _viewport;
        }

        bgfx::setViewRect(0, 0, 0, static_cast<std::uint16_t>(_viewport.width), static_cast<std::uint16_t>(_viewport.height));

        bgfx::touch(0);

        return true;
    }

    void end_frame()
    {
        bgfx::frame();
    }

    [[nodiscard]] int width() const noexcept
    {
        return _viewport.width;
    }

    [[nodiscard]] int height() const noexcept
    {
        return _viewport.height;
    }

    [[nodiscard]] const centui::viewport& viewport() const noexcept
    {
        return _viewport;
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
        if((SDL_GetWindowFlags(_window.get()) & SDL_WINDOW_MINIMIZED) ||
           !SDL_GetWindowSize(_window.get(), &window_width, &window_height) ||
           !SDL_GetWindowSizeInPixels(_window.get(), &pixel_width, &pixel_height) ||
           window_width <= 0 || window_height <= 0 || pixel_width <= 0 || pixel_height <= 0)
        {
            return std::nullopt;
        }

        const auto scale = SDL_GetWindowDisplayScale(_window.get());
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

    detail::sdl_context                   _sdl;
    detail::window_ptr                    _window;
    std::unique_ptr<detail::bgfx_context> _bgfx;

    centui::viewport         _viewport;
    centui::viewport         _backbuffer;
    std::vector<input_event> _input_events;
    std::shared_ptr<host_services> _services;
    bool                     _mouse_captured = false;
    std::uint8_t             _capture_button = 0;
};

} // namespace centui
