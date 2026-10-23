#include <centui/platforms/glfw.hpp>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(CENTUI_GLFW_X11) || defined(CENTUI_GLFW_WAYLAND)
#if defined(CENTUI_GLFW_X11)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#if defined(CENTUI_GLFW_WAYLAND)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#endif
#include <GLFW/glfw3native.h>

namespace centui::glfw::detail
{

context::context()
{
    if(!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

context::~context()
{
    glfwTerminate();
}

std::shared_ptr<context> acquire_context()
{
    static std::weak_ptr<context> active;

    if(auto shared = active.lock())
    {
        return shared;
    }

    auto shared = std::make_shared<context>();
    active = shared;

    return shared;
}

void window_deleter::operator()(GLFWwindow* window) const noexcept
{
    if(window)
    {
        glfwDestroyWindow(window);
    }
}

} // namespace centui::glfw::detail

namespace centui::glfw
{

platform::platform(const window_options& options)
    : m_context(detail::acquire_context()),
      m_owned(create_window(options)),
      m_window(*m_owned)
{
    initialize();
}

platform::platform(GLFWwindow& window)
    : m_window(window)
{
    initialize();
}

platform::~platform()
{
    m_adapters.erase(&m_window);
    restore(glfwSetKeyCallback, m_previous_key, key_callback);
    restore(glfwSetCharCallback, m_previous_char, char_callback);
    restore(glfwSetCursorPosCallback, m_previous_cursor, cursor_callback);
    restore(glfwSetCursorEnterCallback, m_previous_enter, enter_callback);
    restore(glfwSetMouseButtonCallback, m_previous_button, button_callback);
    restore(glfwSetScrollCallback, m_previous_scroll, scroll_callback);
    restore(glfwSetWindowFocusCallback, m_previous_focus, focus_callback);
}

bool platform::poll_events()
{
    m_events.clear();
    m_callback_error = nullptr;
    glfwPollEvents();
    check_callback_error();

    return !glfwWindowShouldClose(&m_window);
}

std::span<const input_event> platform::events() const noexcept
{
    return m_events;
}

std::vector<input_event> platform::take_events()
{
    check_callback_error();

    return std::exchange(m_events, std::vector<input_event>{});
}

centui::viewport platform::viewport() const
{
    int width = 0;
    int height = 0;
    float x_scale = 1;
    glfwGetFramebufferSize(&m_window, &width, &height);
    glfwGetWindowContentScale(&m_window, &x_scale, nullptr);

    return {width, height, x_scale > 0 && std::isfinite(x_scale) ? x_scale : 1};
}

std::shared_ptr<const host_services> platform::services() const noexcept
{
    return m_services;
}

GLFWwindow& platform::native_window() const noexcept
{
    return m_window;
}

centui::native_window_handles platform::native_handles() const
{
#if defined(_WIN32)
    return {glfwGetWin32Window(&m_window), nullptr, native_window_system::win32};
#elif defined(__APPLE__)
    return {glfwGetCocoaWindow(&m_window), nullptr, native_window_system::cocoa};
#elif defined(CENTUI_GLFW_X11) || defined(CENTUI_GLFW_WAYLAND)
#if defined(CENTUI_GLFW_WAYLAND)
    if(glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
    {
        return {glfwGetWaylandWindow(&m_window), glfwGetWaylandDisplay(), native_window_system::wayland};
    }
#endif
#if defined(CENTUI_GLFW_X11)
    if(glfwGetPlatform() == GLFW_PLATFORM_X11)
    {
        return {
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(glfwGetX11Window(&m_window))),
            glfwGetX11Display(),
            native_window_system::x11
        };
    }
#endif
#endif
    throw std::runtime_error("GLFW platform has no supported native window handles");
}

std::unique_ptr<GLFWwindow, detail::window_deleter> platform::create_window(const window_options& options)
{
    if(options.width <= 0 || options.height <= 0)
    {
        throw std::invalid_argument("Window dimensions must be positive");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    std::unique_ptr<GLFWwindow, detail::window_deleter> window{
        glfwCreateWindow(options.width, options.height, options.title.c_str(), nullptr, nullptr)
    };

    if(!window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    return window;
}

void platform::initialize()
{
    if(m_adapters.contains(&m_window))
    {
        throw std::logic_error("A GLFW window already has a CentUI adapter");
    }

    m_services = std::make_shared<host_services>();
    const std::weak_ptr<int> alive = m_lifetime;
    const auto window = std::ref(m_window);
    m_services->read_clipboard = [alive, window]
    {
        if(alive.expired())
        {
            return std::string{};
        }

        const char* value = glfwGetClipboardString(&window.get());

        return value ? std::string{value} : std::string{};
    };
    m_services->write_clipboard = [alive, window](std::string_view value)
    {
        if(!alive.expired())
        {
            glfwSetClipboardString(&window.get(), std::string{value}.c_str());
        }
    };
    m_services->text_input = [](std::optional<rect>) {};

    m_adapters.emplace(&m_window, std::ref(*this));
    m_previous_key = glfwSetKeyCallback(&m_window, key_callback);
    m_previous_char = glfwSetCharCallback(&m_window, char_callback);
    m_previous_cursor = glfwSetCursorPosCallback(&m_window, cursor_callback);
    m_previous_enter = glfwSetCursorEnterCallback(&m_window, enter_callback);
    m_previous_button = glfwSetMouseButtonCallback(&m_window, button_callback);
    m_previous_scroll = glfwSetScrollCallback(&m_window, scroll_callback);
    m_previous_focus = glfwSetWindowFocusCallback(&m_window, focus_callback);
}

void platform::check_callback_error()
{
    if(m_callback_error)
    {
        std::rethrow_exception(std::exchange(m_callback_error, std::exception_ptr{}));
    }
}

modifiers platform::modifiers_now() const noexcept
{
    const auto down = [this](int left, int right)
    {
        return glfwGetKey(&m_window, left) == GLFW_PRESS || glfwGetKey(&m_window, right) == GLFW_PRESS;
    };

    return {
        down(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT),
        down(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL),
        down(GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT),
        down(GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER)
    };
}

key platform::translate_key(int value) noexcept
{
    constexpr std::array mapping{
        std::pair{GLFW_KEY_TAB, key::tab},
        std::pair{GLFW_KEY_ENTER, key::enter},
        std::pair{GLFW_KEY_KP_ENTER, key::enter},
        std::pair{GLFW_KEY_SPACE, key::space},
        std::pair{GLFW_KEY_ESCAPE, key::escape},
        std::pair{GLFW_KEY_LEFT, key::left},
        std::pair{GLFW_KEY_RIGHT, key::right},
        std::pair{GLFW_KEY_UP, key::up},
        std::pair{GLFW_KEY_DOWN, key::down},
        std::pair{GLFW_KEY_HOME, key::home},
        std::pair{GLFW_KEY_END, key::end},
        std::pair{GLFW_KEY_PAGE_UP, key::page_up},
        std::pair{GLFW_KEY_PAGE_DOWN, key::page_down},
        std::pair{GLFW_KEY_BACKSPACE, key::backspace},
        std::pair{GLFW_KEY_DELETE, key::delete_},
        std::pair{GLFW_KEY_A, key::a},
        std::pair{GLFW_KEY_C, key::c},
        std::pair{GLFW_KEY_V, key::v},
        std::pair{GLFW_KEY_X, key::x},
        std::pair{GLFW_KEY_Y, key::y},
        std::pair{GLFW_KEY_Z, key::z}
    };

    for(const auto& [native, portable]: mapping)
    {
        if(value == native)
        {
            return portable;
        }
    }

    return key::unknown;
}

std::pair<float, float> platform::pointer(double x, double y) const
{
    int window_width = 0;
    int window_height = 0;
    glfwGetWindowSize(&m_window, &window_width, &window_height);
    const auto size = viewport();

    return {
        window_width > 0 ? static_cast<float>(x) * size.width / window_width / size.scale : 0,
        window_height > 0 ? static_cast<float>(y) * size.height / window_height / size.scale : 0
    };
}

void platform::key_callback(GLFWwindow* window, int native, int scancode, int action, int mods)
{
    dispatch(
        window,
        [&](platform& self)
        {
            self.m_events.push_back({
                .type = action == GLFW_RELEASE ? input_event_type::key_released : input_event_type::key_pressed,
                .code = translate_key(native),
                .mods =
                    {
                        .shift = bool(mods & GLFW_MOD_SHIFT),
                        .control = bool(mods & GLFW_MOD_CONTROL),
                        .alt = bool(mods & GLFW_MOD_ALT),
                        .system = bool(mods & GLFW_MOD_SUPER),
                    },
                .repeat = action == GLFW_REPEAT,
            });

            if(self.m_previous_key)
            {
                self.m_previous_key(window, native, scancode, action, mods);
            }
        }
    );
}

void platform::char_callback(GLFWwindow* window, unsigned int codepoint)
{
    dispatch(
        window,
        [&](platform& self)
        {
            if(codepoint >= 32 && codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF))
            {
                std::string utf8;

                if(codepoint < 0x80)
                {
                    utf8 += static_cast<char>(codepoint);
                }
                else if(codepoint < 0x800)
                {
                    utf8 += static_cast<char>(0xC0 | (codepoint >> 6));
                    utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                else if(codepoint < 0x10000)
                {
                    utf8 += static_cast<char>(0xE0 | (codepoint >> 12));
                    utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                else
                {
                    utf8 += static_cast<char>(0xF0 | (codepoint >> 18));
                    utf8 += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (codepoint & 0x3F));
                }

                self.m_events.push_back({.type = input_event_type::text_entered, .text = std::move(utf8)});
            }

            if(self.m_previous_char)
            {
                self.m_previous_char(window, codepoint);
            }
        }
    );
}

void platform::cursor_callback(GLFWwindow* window, double x, double y)
{
    dispatch(
        window,
        [&](platform& self)
        {
            const auto [logical_x, logical_y] = self.pointer(x, y);
            self.m_events.push_back(
                {.type = input_event_type::pointer_moved, .x = logical_x, .y = logical_y, .mods = self.modifiers_now()}
            );

            if(self.m_previous_cursor)
            {
                self.m_previous_cursor(window, x, y);
            }
        }
    );
}

void platform::enter_callback(GLFWwindow* window, int entered)
{
    dispatch(
        window,
        [&](platform& self)
        {
            if(!entered)
            {
                self.m_events.push_back({.type = input_event_type::pointer_left});
            }

            if(self.m_previous_enter)
            {
                self.m_previous_enter(window, entered);
            }
        }
    );
}

void platform::button_callback(GLFWwindow* window, int native, int action, int mods)
{
    dispatch(
        window,
        [&](platform& self)
        {
            double x = 0;
            double y = 0;
            glfwGetCursorPos(window, &x, &y);
            const auto [logical_x, logical_y] = self.pointer(x, y);
            const auto button = native == GLFW_MOUSE_BUTTON_LEFT     ? mouse_button::primary
                                : native == GLFW_MOUSE_BUTTON_RIGHT  ? mouse_button::secondary
                                : native == GLFW_MOUSE_BUTTON_MIDDLE ? mouse_button::middle
                                                                     : mouse_button::other;
            self.m_events.push_back({
                .type = action == GLFW_PRESS ? input_event_type::pointer_pressed : input_event_type::pointer_released,
                .x = logical_x,
                .y = logical_y,
                .button = button,
                .mods = {
                    bool(mods & GLFW_MOD_SHIFT),
                    bool(mods & GLFW_MOD_CONTROL),
                    bool(mods & GLFW_MOD_ALT),
                    bool(mods & GLFW_MOD_SUPER)
                },
            });

            if(self.m_previous_button)
            {
                self.m_previous_button(window, native, action, mods);
            }
        }
    );
}

void platform::scroll_callback(GLFWwindow* window, double horizontal, double vertical)
{
    dispatch(
        window,
        [&](platform& self)
        {
            double x = 0;
            double y = 0;
            glfwGetCursorPos(window, &x, &y);
            const auto [logical_x, logical_y] = self.pointer(x, y);
            self.m_events.push_back({
                .type = input_event_type::wheel,
                .x = logical_x,
                .y = logical_y,
                .mods = self.modifiers_now(),
                .delta_x = static_cast<float>(-horizontal * 40),
                .delta_y = static_cast<float>(-vertical * 40),
            });

            if(self.m_previous_scroll)
            {
                self.m_previous_scroll(window, horizontal, vertical);
            }
        }
    );
}

void platform::focus_callback(GLFWwindow* window, int focused)
{
    dispatch(
        window,
        [&](platform& self)
        {
            self.m_events.push_back({.type = focused ? input_event_type::focus_gained : input_event_type::focus_lost});

            if(self.m_previous_focus)
            {
                self.m_previous_focus(window, focused);
            }
        }
    );
}

} // namespace centui::glfw
