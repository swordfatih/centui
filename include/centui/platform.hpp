#pragma once

#include <memory>
#include <span>
#include <string>

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

enum class native_window_system
{
    win32,
    cocoa,
    android,
    x11,
    wayland,
    web
};

/// \brief Borrowed native C handles or a browser canvas selector used to connect a renderer.
/// These handles never own resources; their platform must outlive the renderer.
struct native_window_handles
{
    void* window = nullptr;
    void* display = nullptr;
    native_window_system system = native_window_system::win32;
    const char* canvas = nullptr;
};

/// \brief Supplies window events, framebuffer dimensions, and optional host services.
class platform
{
public:
    virtual ~platform() = default;

    [[nodiscard]] virtual bool poll_events() = 0;

    [[nodiscard]] virtual std::span<const input_event> events() const noexcept = 0;

    [[nodiscard]] virtual centui::viewport viewport() const = 0;

    [[nodiscard]] virtual std::shared_ptr<const host_services> services() const noexcept = 0;
};

} // namespace centui
