#include <centui/platforms/sdl.hpp>

namespace centui::detail
{

sdl_context::sdl_context()
{
    if(!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        throw std::runtime_error(SDL_GetError());
    }
}

sdl_context::~sdl_context()
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void window_deleter::operator()(SDL_Window* window) const noexcept
{
    if(window)
    {
        SDL_DestroyWindow(window);
    }
}

} // namespace centui::detail

namespace centui::sdl
{

platform::platform(const window_options& options)
    : m_sdl(std::make_unique<detail::sdl_context>()),
      m_owned(create_window(options)),
      m_window(*m_owned)
{
    initialize_services();
}

platform::platform(SDL_Window& window)
    : m_window(window)
{
    initialize_services();
}

detail::window_ptr platform::create_window(const window_options& options)
{
    if(options.width <= 0 || options.height <= 0)
    {
        throw std::invalid_argument("Window dimensions must be positive");
    }

    detail::window_ptr window{SDL_CreateWindow(
        options.title.c_str(),
        options.width,
        options.height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    )};

    if(!window)
    {
        throw std::runtime_error(SDL_GetError());
    }

    return window;
}

void platform::initialize_services()
{
    m_services = std::make_shared<host_services>();
    const std::weak_ptr<std::reference_wrapper<SDL_Window>> weak_window = m_lifetime;
    m_services->read_clipboard = [weak_window]
    {
        if(weak_window.expired())
        {
            return std::string{};
        }

        const auto text = std::shared_ptr<char>{SDL_GetClipboardText(), SDL_free};

        return text ? std::string{text.get()} : std::string{};
    };
    m_services->write_clipboard = [weak_window](std::string_view text)
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
    m_services->text_input = [weak_window](std::optional<rect> area)
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
            const SDL_Rect input_area{
                static_cast<int>(std::floor(area->x * x_scale)),
                static_cast<int>(std::floor(area->y * y_scale)),
                std::max(1, static_cast<int>(std::ceil(area->width * x_scale))),
                std::max(1, static_cast<int>(std::ceil(area->height * y_scale)))
            };
            SDL_SetTextInputArea(std::addressof(window->get()), &input_area, 0);
        }
    };
}

platform::~platform()
{
    release_mouse_capture();
}

std::shared_ptr<const host_services> platform::services() const noexcept
{
    return m_services;
}

bool platform::poll_events()
{
    return poll_events({});
}

bool platform::poll_events(const std::function<void(const SDL_Event&)>& on_event)
{
    m_input_events.clear();
    SDL_Event event;
    bool running = true;

    while(SDL_PollEvent(&event))
    {
        if(on_event)
        {
            on_event(event);
        }

        if(event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                            event.window.windowID == SDL_GetWindowID(std::addressof(m_window))))
        {
            running = false;
        }

        if(auto translated = translate(event))
        {
            m_input_events.push_back(std::move(*translated));
        }
    }

    return running;
}

std::optional<input_event> platform::translate(const SDL_Event& event)
{
    if(event.type == SDL_EVENT_QUIT)
    {
        return std::nullopt;
    }

    if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
       event.window.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        return std::nullopt;
    }

    if((event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED ||
        event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) &&
       event.window.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        if(event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED)
        {
            release_mouse_capture();

            return input_event{
                .type = event.type == SDL_EVENT_WINDOW_FOCUS_LOST ? input_event_type::focus_lost
                                                                  : input_event_type::pointer_cancelled,
            };
        }

        if(event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
        {
            return input_event{.type = input_event_type::focus_gained};
        }
    }

    if(event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE && event.window.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        return input_event{.type = input_event_type::pointer_left};
    }

    if((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) &&
       event.key.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        return input_event{
            .type = event.type == SDL_EVENT_KEY_DOWN ? input_event_type::key_pressed : input_event_type::key_released,
            .code = translate_key(event.key.key),
            .mods = translate_modifiers(event.key.mod),
            .repeat = event.key.repeat,
        };
    }

    if(event.type == SDL_EVENT_TEXT_INPUT && event.text.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        return input_event{.type = input_event_type::text_entered, .text = event.text.text ? event.text.text : ""};
    }

    if(event.type == SDL_EVENT_TEXT_EDITING && event.edit.windowID == SDL_GetWindowID(std::addressof(m_window)))
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

        return input_event{
            .type = input_event_type::composition,
            .text = text,
            .selection_start = start,
            .selection_length = end - start,
        };
    }

    if(event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        if(const auto position = to_logical_position(event.wheel.mouse_x, event.wheel.mouse_y))
        {
            const auto direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -40.0f : 40.0f;

            return input_event{
                .type = input_event_type::wheel,
                .x = position->first,
                .y = position->second,
                .mods = translate_modifiers(SDL_GetModState()),
                .delta_x = event.wheel.x * direction,
                .delta_y = -event.wheel.y * direction,
            };
        }

        return std::nullopt;
    }

    if(event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        if(const auto position = to_logical_position(event.motion.x, event.motion.y))
        {
            return input_event{
                .type = input_event_type::pointer_moved,
                .x = position->first,
                .y = position->second,
                .mods = translate_modifiers(SDL_GetModState()),
            };
        }

        return std::nullopt;
    }

    if((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
       event.button.windowID == SDL_GetWindowID(std::addressof(m_window)))
    {
        const auto type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? input_event_type::pointer_pressed
                                                                    : input_event_type::pointer_released;

        if(type == input_event_type::pointer_released && event.button.button == m_capture_button)
        {
            release_mouse_capture();
        }

        if(const auto position = to_logical_position(event.button.x, event.button.y))
        {
            if(type == input_event_type::pointer_pressed && !m_mouse_captured)
            {
                m_mouse_captured = SDL_CaptureMouse(true);
                m_capture_button = event.button.button;
            }

            return input_event{
                .type = type,
                .x = position->first,
                .y = position->second,
                .button = translate_button(event.button.button),
                .mods = translate_modifiers(SDL_GetModState()),
            };
        }
        else
        {
            return input_event{.type = input_event_type::pointer_cancelled};
        }
    }

    return std::nullopt;
}

std::span<const input_event> platform::events() const noexcept
{
    return m_input_events;
}

centui::viewport platform::viewport() const
{
    int width = 0;
    int height = 0;

    if((SDL_GetWindowFlags(std::addressof(m_window)) & SDL_WINDOW_MINIMIZED) ||
       !SDL_GetWindowSizeInPixels(std::addressof(m_window), &width, &height))
    {
        return {};
    }

    return {width, height, SDL_GetWindowDisplayScale(std::addressof(m_window))};
}

SDL_Window& platform::native_window() const noexcept
{
    return m_window;
}

centui::native_window_handles platform::native_handles() const
{
    const auto properties = SDL_GetWindowProperties(std::addressof(m_window));

    if(properties == 0)
    {
        throw std::runtime_error(SDL_GetError());
    }

    if(const auto window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr))
    {
        return {window, nullptr, native_window_system::win32};
    }

    if(const auto window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr))
    {
        return {window, nullptr, native_window_system::cocoa};
    }

    if(const auto window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr))
    {
        return {window, nullptr, native_window_system::android};
    }

    if(const auto window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr))
    {
        const auto display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);

        if(!display)
        {
            throw std::runtime_error("SDL did not expose the Wayland display");
        }

        return {window, display, native_window_system::wayland};
    }

    const auto window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

    if(window != 0)
    {
        const auto display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);

        if(!display)
        {
            throw std::runtime_error("SDL did not expose the X11 display");
        }

        return {reinterpret_cast<void*>(static_cast<std::uintptr_t>(window)), display, native_window_system::x11};
    }

    throw std::runtime_error("SDL video driver has no supported native window properties");
}

modifiers platform::translate_modifiers(SDL_Keymod value)
{
    return {
        .shift = (value & SDL_KMOD_SHIFT) != 0,
        .control = (value & SDL_KMOD_CTRL) != 0,
        .alt = (value & SDL_KMOD_ALT) != 0,
        .system = (value & SDL_KMOD_GUI) != 0,
    };
}

key platform::translate_key(SDL_Keycode value)
{
    static constexpr std::array pairs{
        std::pair{SDLK_TAB, key::tab},
        std::pair{SDLK_RETURN, key::enter},
        std::pair{SDLK_KP_ENTER, key::enter},
        std::pair{SDLK_SPACE, key::space},
        std::pair{SDLK_ESCAPE, key::escape},
        std::pair{SDLK_LEFT, key::left},
        std::pair{SDLK_RIGHT, key::right},
        std::pair{SDLK_UP, key::up},
        std::pair{SDLK_DOWN, key::down},
        std::pair{SDLK_HOME, key::home},
        std::pair{SDLK_END, key::end},
        std::pair{SDLK_PAGEUP, key::page_up},
        std::pair{SDLK_PAGEDOWN, key::page_down},
        std::pair{SDLK_BACKSPACE, key::backspace},
        std::pair{SDLK_DELETE, key::delete_},
        std::pair{SDLK_A, key::a},
        std::pair{SDLK_C, key::c},
        std::pair{SDLK_V, key::v},
        std::pair{SDLK_X, key::x},
        std::pair{SDLK_Y, key::y},
        std::pair{SDLK_Z, key::z}
    };

    for(const auto& [native, translated]: pairs)
    {
        if(value == native)
        {
            return translated;
        }
    }

    return key::unknown;
}

void platform::release_mouse_capture() noexcept
{
    if(m_mouse_captured)
    {
        SDL_CaptureMouse(false);
        m_mouse_captured = false;
    }
}

std::optional<std::pair<float, float>> platform::to_logical_position(float x, float y) const
{
    int window_width = 0;
    int window_height = 0;
    int pixel_width = 0;
    int pixel_height = 0;

    if((SDL_GetWindowFlags(std::addressof(m_window)) & SDL_WINDOW_MINIMIZED) ||
       !SDL_GetWindowSize(std::addressof(m_window), &window_width, &window_height) ||
       !SDL_GetWindowSizeInPixels(std::addressof(m_window), &pixel_width, &pixel_height) || window_width <= 0 ||
       window_height <= 0 || pixel_width <= 0 || pixel_height <= 0)
    {
        return std::nullopt;
    }

    const auto scale = SDL_GetWindowDisplayScale(std::addressof(m_window));

    return std::pair{
        x * static_cast<float>(pixel_width) / static_cast<float>(window_width) / scale,
        y * static_cast<float>(pixel_height) / static_cast<float>(window_height) / scale
    };
}

mouse_button platform::translate_button(std::uint8_t button) noexcept
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

} // namespace centui::sdl
