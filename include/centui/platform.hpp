#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <centui/types.hpp>

namespace centui
{

/// \brief Initial title and framebuffer dimensions of an owned window.
struct window_options
{
    std::string title{};
    int width = 800;
    int height = 600;
};

enum class surface_handle_type
{
    native,
    wayland
};

/// \brief Borrowed platform handles for a window, view, or layer used for rendering.
/// The platform and the underlying surface must outlive the renderer.
struct native_surface
{
    void* handle = nullptr;
    void* display = nullptr;
    surface_handle_type type = surface_handle_type::native;
};

/// \brief An owned identifier for a platform rendering surface.
struct named_surface
{
    std::string name;
};

using render_surface = std::variant<native_surface, named_surface>;

/// \brief Supplies window events, framebuffer dimensions, and optional host services.
class platform
{
public:
    platform();

    virtual ~platform();

    platform(const platform&) = delete;
    platform& operator=(const platform&) = delete;

    [[nodiscard]] virtual bool poll_events() = 0;

    /// \brief Transfers queued events exactly once. Does not poll native events.
    [[nodiscard]] virtual std::vector<platform_event> take_events();

    [[nodiscard]] virtual centui::viewport viewport() const = 0;

    [[nodiscard]] virtual std::shared_ptr<const host_services> services() const noexcept = 0;

    [[nodiscard]] platform_state state() const;

    /// \brief Returns a graphics connection when this adapter supports external renderers.
    /// Native handles are borrowed; named identifiers are returned by value.
    [[nodiscard]] virtual std::optional<render_surface> surface() const
    {
        return std::nullopt;
    }

protected:
    void enqueue(platform_event event);
    void enqueue(const input_event& event);
    void refresh_state();
    void set_activity(application_activity activity);
    void set_surface_available(bool available);
    void invalidate_resources();
    void set_safe_area(safe_area_insets insets);
    void set_keyboard_occlusion(std::optional<rect> area);
    void set_initial_focus(bool focused);
    [[nodiscard]] bool surface_available() const noexcept;

    struct service_request
    {
        request_id id;
        // Safe to retain in asynchronous platform code; never calls a widget.
        std::function<void(service_status, std::string)> complete;
    };

    [[nodiscard]] service_request begin_request() const;
    [[nodiscard]] std::shared_ptr<host_services> clipboard_services(
        std::function<std::string()> read,
        std::function<void(std::string_view)> write
    ) const;

private:
    struct mailbox;
    static service_request make_request(std::weak_ptr<mailbox> queue);
    std::shared_ptr<mailbox> m_mailbox;
    platform_state m_state;
    std::optional<platform_state> m_reported;
    bool m_surface_available = true;
};

} // namespace centui
