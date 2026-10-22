module;

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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
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

export module centui.glfw;
export import centui.platform;

namespace centui::glfw::detail
{
struct context
{
    context()
    {
        if(!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    ~context()
    {
        glfwTerminate();
    }
};

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

struct window_deleter
{
    void operator()(GLFWwindow* window) const noexcept
    {
        if(window)
        {
            glfwDestroyWindow(window);
        }
    }
};
} // namespace centui::glfw::detail

export namespace centui::glfw
{
class platform final : public centui::platform
{
public:
    explicit platform(const window_options& options)
        : _context(detail::acquire_context()), _owned(create_window(options)), _window(*_owned)
    {
        initialize();
    }

    explicit platform(GLFWwindow& window) : _window(window)
    {
        initialize();
    }

    ~platform() override
    {
        _adapters.erase(&_window);
        restore(glfwSetKeyCallback, _previous_key, key_callback);
        restore(glfwSetCharCallback, _previous_char, char_callback);
        restore(glfwSetCursorPosCallback, _previous_cursor, cursor_callback);
        restore(glfwSetCursorEnterCallback, _previous_enter, enter_callback);
        restore(glfwSetMouseButtonCallback, _previous_button, button_callback);
        restore(glfwSetScrollCallback, _previous_scroll, scroll_callback);
        restore(glfwSetWindowFocusCallback, _previous_focus, focus_callback);
    }

    platform(const platform&) = delete;
    platform& operator=(const platform&) = delete;
    platform(platform&&) = delete;
    platform& operator=(platform&&) = delete;

    [[nodiscard]] bool poll_events() override
    {
        _events.clear();
        _callback_error = nullptr;
        glfwPollEvents();
        check_callback_error();
        return !glfwWindowShouldClose(&_window);
    }

    [[nodiscard]] std::span<const input_event> events() const noexcept override
    {
        return _events;
    }

    // Use this after an application-owned glfwPollEvents() call.
    [[nodiscard]] std::vector<input_event> take_events()
    {
        check_callback_error();
        return std::exchange(_events, std::vector<input_event>{});
    }

    [[nodiscard]] centui::viewport viewport() const override
    {
        int width = 0;
        int height = 0;
        float x_scale = 1;
        glfwGetFramebufferSize(&_window, &width, &height);
        glfwGetWindowContentScale(&_window, &x_scale, nullptr);
        return {width, height, x_scale > 0 && std::isfinite(x_scale) ? x_scale : 1};
    }

    [[nodiscard]] std::shared_ptr<const host_services> services() const noexcept override
    {
        return _services;
    }

    [[nodiscard]] GLFWwindow& native_window() const noexcept
    {
        return _window;
    }

    [[nodiscard]] centui::native_window_handles native_handles() const
    {
#if defined(_WIN32)
        return {glfwGetWin32Window(&_window), nullptr, native_window_system::win32};
#elif defined(__APPLE__)
        return {glfwGetCocoaWindow(&_window), nullptr, native_window_system::cocoa};
#elif defined(CENTUI_GLFW_X11) || defined(CENTUI_GLFW_WAYLAND)
#if defined(CENTUI_GLFW_WAYLAND)
        if(glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        {
            return {glfwGetWaylandWindow(&_window), glfwGetWaylandDisplay(), native_window_system::wayland};
        }
#endif
#if defined(CENTUI_GLFW_X11)
        if(glfwGetPlatform() == GLFW_PLATFORM_X11)
        {
            return {reinterpret_cast<void*>(static_cast<std::uintptr_t>(glfwGetX11Window(&_window))),
                    glfwGetX11Display(), native_window_system::x11};
        }
#endif
#endif
        throw std::runtime_error("GLFW platform has no supported native window handles");
    }

private:
    static std::unique_ptr<GLFWwindow, detail::window_deleter> create_window(const window_options& options)
    {
        if(options.width <= 0 || options.height <= 0)
        {
            throw std::invalid_argument("Window dimensions must be positive");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        std::unique_ptr<GLFWwindow, detail::window_deleter> window{
            glfwCreateWindow(options.width, options.height, options.title.c_str(), nullptr, nullptr)};
        if(!window)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }
        return window;
    }

    void initialize()
    {
        if(_adapters.contains(&_window))
        {
            throw std::logic_error("A GLFW window already has a CentUI adapter");
        }
        _services = std::make_shared<host_services>();
        const std::weak_ptr<int> alive = _lifetime;
        GLFWwindow* window = &_window;
        _services->read_clipboard = [alive, window]
        {
            if(alive.expired())
            {
                return std::string{};
            }
            const char* value = glfwGetClipboardString(window);
            return value ? std::string{value} : std::string{};
        };
        _services->write_clipboard = [alive, window](std::string_view value)
        {
            if(!alive.expired())
            {
                glfwSetClipboardString(window, std::string{value}.c_str());
            }
        };
        _services->text_input = [](std::optional<rect>)
        {
        };

        _adapters.emplace(&_window, this);
        _previous_key = glfwSetKeyCallback(&_window, key_callback);
        _previous_char = glfwSetCharCallback(&_window, char_callback);
        _previous_cursor = glfwSetCursorPosCallback(&_window, cursor_callback);
        _previous_enter = glfwSetCursorEnterCallback(&_window, enter_callback);
        _previous_button = glfwSetMouseButtonCallback(&_window, button_callback);
        _previous_scroll = glfwSetScrollCallback(&_window, scroll_callback);
        _previous_focus = glfwSetWindowFocusCallback(&_window, focus_callback);
    }

    template <typename Setter, typename Callback>
    void restore(Setter setter, Callback previous, Callback installed) noexcept
    {
        const auto current = setter(&_window, previous);
        if(current != installed)
        {
            setter(&_window, current);
        }
    }

    template <typename Callback>
    static void dispatch(GLFWwindow* window, Callback&& callback) noexcept
    {
        const auto found = _adapters.find(window);
        if(found == _adapters.end())
        {
            return;
        }
        auto& self = *found->second;
        try
        {
            callback(self);
        }
        catch(...)
        {
            self._callback_error = std::current_exception();
        }
    }

    void check_callback_error()
    {
        if(_callback_error)
        {
            std::rethrow_exception(std::exchange(_callback_error, std::exception_ptr{}));
        }
    }

    [[nodiscard]] modifiers modifiers_now() const noexcept
    {
        const auto down = [this](int left, int right)
        {
            return glfwGetKey(&_window, left) == GLFW_PRESS ||
                   glfwGetKey(&_window, right) == GLFW_PRESS;
        };
        return {down(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT),
                down(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL),
                down(GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT),
                down(GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER)};
    }

    [[nodiscard]] static key translate_key(int value) noexcept
    {
        constexpr std::array mapping{
            std::pair{GLFW_KEY_TAB, key::tab}, std::pair{GLFW_KEY_ENTER, key::enter},
            std::pair{GLFW_KEY_KP_ENTER, key::enter}, std::pair{GLFW_KEY_SPACE, key::space},
            std::pair{GLFW_KEY_ESCAPE, key::escape}, std::pair{GLFW_KEY_LEFT, key::left},
            std::pair{GLFW_KEY_RIGHT, key::right}, std::pair{GLFW_KEY_UP, key::up},
            std::pair{GLFW_KEY_DOWN, key::down}, std::pair{GLFW_KEY_HOME, key::home},
            std::pair{GLFW_KEY_END, key::end}, std::pair{GLFW_KEY_PAGE_UP, key::page_up},
            std::pair{GLFW_KEY_PAGE_DOWN, key::page_down}, std::pair{GLFW_KEY_BACKSPACE, key::backspace},
            std::pair{GLFW_KEY_DELETE, key::delete_}, std::pair{GLFW_KEY_A, key::a},
            std::pair{GLFW_KEY_C, key::c}, std::pair{GLFW_KEY_V, key::v},
            std::pair{GLFW_KEY_X, key::x}, std::pair{GLFW_KEY_Y, key::y},
            std::pair{GLFW_KEY_Z, key::z}};
        for(const auto& [native, portable]: mapping)
        {
            if(value == native)
            {
                return portable;
            }
        }
        return key::unknown;
    }

    [[nodiscard]] std::pair<float, float> pointer(double x, double y) const
    {
        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(&_window, &window_width, &window_height);
        const auto size = viewport();
        return {window_width > 0 ? static_cast<float>(x) * size.width / window_width / size.scale : 0,
                window_height > 0 ? static_cast<float>(y) * size.height / window_height / size.scale : 0};
    }

    static void key_callback(GLFWwindow* window, int native, int scancode, int action, int mods)
    {
        dispatch(window, [&](platform& self)
        {
            self._events.push_back({.type = action == GLFW_RELEASE ? input_event_type::key_released : input_event_type::key_pressed,
                                    .code = translate_key(native),
                                    .mods = {bool(mods & GLFW_MOD_SHIFT), bool(mods & GLFW_MOD_CONTROL),
                                             bool(mods & GLFW_MOD_ALT), bool(mods & GLFW_MOD_SUPER)},
                                    .repeat = action == GLFW_REPEAT});
            if(self._previous_key)
            {
                self._previous_key(window, native, scancode, action, mods);
            }
        });
    }

    static void char_callback(GLFWwindow* window, unsigned int codepoint)
    {
        dispatch(window, [&](platform& self)
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
                self._events.push_back({.type = input_event_type::text_entered, .text = std::move(utf8)});
            }
            if(self._previous_char)
            {
                self._previous_char(window, codepoint);
            }
        });
    }

    static void cursor_callback(GLFWwindow* window, double x, double y)
    {
        dispatch(window, [&](platform& self)
        {
            const auto [logical_x, logical_y] = self.pointer(x, y);
            self._events.push_back({.type = input_event_type::pointer_moved, .x = logical_x, .y = logical_y,
                                    .mods = self.modifiers_now()});
            if(self._previous_cursor)
            {
                self._previous_cursor(window, x, y);
            }
        });
    }

    static void enter_callback(GLFWwindow* window, int entered)
    {
        dispatch(window, [&](platform& self)
        {
            if(!entered)
            {
                self._events.push_back({.type = input_event_type::pointer_left});
            }
            if(self._previous_enter)
            {
                self._previous_enter(window, entered);
            }
        });
    }

    static void button_callback(GLFWwindow* window, int native, int action, int mods)
    {
        dispatch(window, [&](platform& self)
        {
            double x = 0;
            double y = 0;
            glfwGetCursorPos(window, &x, &y);
            const auto [logical_x, logical_y] = self.pointer(x, y);
            const auto button = native == GLFW_MOUSE_BUTTON_LEFT ? mouse_button::primary :
                                native == GLFW_MOUSE_BUTTON_RIGHT ? mouse_button::secondary :
                                native == GLFW_MOUSE_BUTTON_MIDDLE ? mouse_button::middle : mouse_button::other;
            self._events.push_back({.type = action == GLFW_PRESS ? input_event_type::pointer_pressed : input_event_type::pointer_released,
                                    .x = logical_x, .y = logical_y, .button = button,
                                    .mods = {bool(mods & GLFW_MOD_SHIFT), bool(mods & GLFW_MOD_CONTROL),
                                             bool(mods & GLFW_MOD_ALT), bool(mods & GLFW_MOD_SUPER)}});
            if(self._previous_button)
            {
                self._previous_button(window, native, action, mods);
            }
        });
    }

    static void scroll_callback(GLFWwindow* window, double horizontal, double vertical)
    {
        dispatch(window, [&](platform& self)
        {
            double x = 0;
            double y = 0;
            glfwGetCursorPos(window, &x, &y);
            const auto [logical_x, logical_y] = self.pointer(x, y);
            self._events.push_back({.type = input_event_type::wheel, .x = logical_x, .y = logical_y,
                                    .mods = self.modifiers_now(), .delta_x = static_cast<float>(-horizontal * 40),
                                    .delta_y = static_cast<float>(-vertical * 40)});
            if(self._previous_scroll)
            {
                self._previous_scroll(window, horizontal, vertical);
            }
        });
    }

    static void focus_callback(GLFWwindow* window, int focused)
    {
        dispatch(window, [&](platform& self)
        {
            self._events.push_back({.type = focused ? input_event_type::focus_gained : input_event_type::focus_lost});
            if(self._previous_focus)
            {
                self._previous_focus(window, focused);
            }
        });
    }

    std::shared_ptr<detail::context> _context;
    std::unique_ptr<GLFWwindow, detail::window_deleter> _owned;
    GLFWwindow& _window;
    std::shared_ptr<int> _lifetime = std::make_shared<int>(0);
    std::shared_ptr<host_services> _services;
    std::vector<input_event> _events;
    std::exception_ptr _callback_error;
    GLFWkeyfun _previous_key = nullptr;
    GLFWcharfun _previous_char = nullptr;
    GLFWcursorposfun _previous_cursor = nullptr;
    GLFWcursorenterfun _previous_enter = nullptr;
    GLFWmousebuttonfun _previous_button = nullptr;
    GLFWscrollfun _previous_scroll = nullptr;
    GLFWwindowfocusfun _previous_focus = nullptr;
    inline static std::unordered_map<GLFWwindow*, platform*> _adapters;
};
} // namespace centui::glfw
