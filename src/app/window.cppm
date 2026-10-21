
module;

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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

using window_ptr = std::unique_ptr<SDL_Window, window_deleter>;

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
        _window.reset(SDL_CreateWindow(
            title.c_str(),
            width,
            height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));

        if(!_window)
        {
            throw std::runtime_error(SDL_GetError());
        }

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

            if(event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == SDL_GetWindowID(_window.get()))
            {
                if(const auto position = to_pixel_position(event.motion.x, event.motion.y))
                {
                    _input_events.push_back({.type = input_event_type::pointer_moved, .x = position->first, .y = position->second});
                }
                continue;
            }

            if((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
               event.button.windowID == SDL_GetWindowID(_window.get()))
            {
                const auto type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? input_event_type::pointer_pressed : input_event_type::pointer_released;
                if(type == input_event_type::pointer_released && event.button.button == SDL_BUTTON_LEFT)
                {
                    release_mouse_capture();
                }
                if(const auto position = to_pixel_position(event.button.x, event.button.y))
                {
                    if(type == input_event_type::pointer_pressed && event.button.button == SDL_BUTTON_LEFT)
                    {
                        _mouse_captured = SDL_CaptureMouse(true);
                    }
                    _input_events.push_back({.type = type,
                                             .x = position->first,
                                             .y = position->second,
                                             .button = translate_button(event.button.button)});
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
        _viewport = {width, height};
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
    void release_mouse_capture() noexcept
    {
        if(_mouse_captured)
        {
            SDL_CaptureMouse(false);
            _mouse_captured = false;
        }
    }

    [[nodiscard]] std::optional<std::pair<float, float>> to_pixel_position(float x, float y) const
    {
        int logical_width = 0;
        int logical_height = 0;
        int pixel_width = 0;
        int pixel_height = 0;
        if((SDL_GetWindowFlags(_window.get()) & SDL_WINDOW_MINIMIZED) ||
           !SDL_GetWindowSize(_window.get(), &logical_width, &logical_height) ||
           !SDL_GetWindowSizeInPixels(_window.get(), &pixel_width, &pixel_height) ||
           logical_width <= 0 || logical_height <= 0 || pixel_width <= 0 || pixel_height <= 0)
        {
            return std::nullopt;
        }

        return std::pair{
            x * static_cast<float>(pixel_width) / static_cast<float>(logical_width),
            y * static_cast<float>(pixel_height) / static_cast<float>(logical_height)};
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
    bool                     _mouse_captured = false;
};

} // namespace centui
