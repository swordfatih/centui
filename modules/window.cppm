
module;

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

export module centui.window;

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
    window(const char* title, int width, int height)
    {
        _window.reset(SDL_CreateWindow(
            title,
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

        if(!SDL_GetWindowSizeInPixels(_window.get(), &_width, &_height))
        {
            throw std::runtime_error(SDL_GetError());
        }

        if(_width <= 0 || _height <= 0)
        {
            throw std::runtime_error("Invalid window pixel dimensions");
        }

        bgfx::Init init;
        init.swapChain.nwh = native_window;
        init.swapChain.width = static_cast<std::uint32_t>(_width);
        init.swapChain.height = static_cast<std::uint32_t>(_height);
        init.reset = BGFX_RESET_VSYNC;

        _bgfx = std::make_unique<detail::bgfx_context>(init);

        bgfx::setViewClear(
            0,
            BGFX_CLEAR_COLOR,
            0x202840FF);
    }

    ~window() = default;

    window(const window&) = delete;
    window& operator=(const window&) = delete;

    window(window&&) = delete;
    window& operator=(window&&) = delete;

    [[nodiscard]] bool poll_events()
    {
        SDL_Event event;

        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_EVENT_QUIT)
            {
                return false;
            }

            if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
               event.window.windowID == SDL_GetWindowID(_window.get()))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool begin_frame()
    {
        int width = 0;
        int height = 0;

        if(!SDL_GetWindowSizeInPixels(_window.get(), &width, &height))
        {
            return false;
        }

        if(width <= 0 || height <= 0)
        {
            return false;
        }

        if(width != _width || height != _height)
        {
            _width = width;
            _height = height;

            bgfx::SwapChain swap_chain;
            swap_chain.width = static_cast<std::uint32_t>(_width);
            swap_chain.height = static_cast<std::uint32_t>(_height);

            bgfx::reset(BGFX_RESET_VSYNC, &swap_chain);
        }

        bgfx::setViewRect(0, 0, 0, static_cast<std::uint16_t>(_width), static_cast<std::uint16_t>(_height));

        bgfx::touch(0);

        return true;
    }

    void end_frame()
    {
        bgfx::frame();
    }

    [[nodiscard]] int width() const noexcept
    {
        return _width;
    }

    [[nodiscard]] int height() const noexcept
    {
        return _height;
    }

private:
    detail::sdl_context                   _sdl;
    detail::window_ptr                    _window;
    std::unique_ptr<detail::bgfx_context> _bgfx;

    int _width = 0;
    int _height = 0;
};

} // namespace centui
