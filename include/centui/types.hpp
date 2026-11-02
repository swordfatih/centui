#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <centypo/types.hpp>

namespace centui
{

/// \brief An RGBA color with 8-bit channels.
struct color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    static constexpr color from_hex(std::uint32_t value)
    {
        return {
            static_cast<std::uint8_t>((value >> 16) & 0xff),
            static_cast<std::uint8_t>((value >> 8) & 0xff),
            static_cast<std::uint8_t>(value & 0xff),
            255
        };
    }

    constexpr bool operator==(const color&) const = default;
};

/// \brief Colors selected from the current widget interaction state.
struct states_color
{
    color normal{};
    color hover{};
    color pressed{};
    color focused{};
    color disabled{};

    static constexpr states_color all(color value)
    {
        return {value, value, value, value, value};
    }

    constexpr states_color& operator=(color value)
    {
        return *this = all(value);
    }

    constexpr bool operator==(const states_color&) const = default;
};

/// \brief Framebuffer dimensions and the scale from logical units to pixels.
struct viewport
{
    // Framebuffer pixels; widget geometry uses logical units.
    int width = 0;
    int height = 0;
    float scale = 1.0f;

    [[nodiscard]] constexpr float logical_width() const noexcept
    {
        return width / scale;
    }

    [[nodiscard]] constexpr float logical_height() const noexcept
    {
        return height / scale;
    }

    constexpr bool operator==(const viewport&) const = default;
};

enum class input_event_type
{
    pointer_moved,
    pointer_pressed,
    pointer_released,
    pointer_left,
    pointer_cancelled,
    focus_lost,
    focus_gained,
    wheel,
    key_pressed,
    key_released,
    text_entered,
    composition
};

enum class key
{
    unknown,
    tab,
    enter,
    space,
    escape,
    left,
    right,
    up,
    down,
    home,
    end,
    page_up,
    page_down,
    backspace,
    delete_,
    a,
    c,
    v,
    x,
    y,
    z
};

struct modifiers
{
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool system = false;
};

enum class mouse_button
{
    primary,
    secondary,
    middle,
    other
};

using pointer_id = std::uint64_t;

enum class pointer_device
{
    mouse,
    touch,
    pen
};

/// \brief A portable input event; pointer coordinates use logical units.
struct input_event
{
    input_event_type type;
    float x = 0;
    float y = 0;
    mouse_button button = mouse_button::primary;
    centui::key code = key::unknown;
    modifiers mods{};
    bool repeat = false;
    float delta_x = 0;
    float delta_y = 0;
    std::string text{};
    std::size_t selection_start = 0;
    std::size_t selection_length = 0;
    pointer_id pointer = 0;
    pointer_device device = pointer_device::mouse;
    bool hover_capable = true;
    std::chrono::nanoseconds timestamp{};
    // Supplied by document routing for releases, including clipping and occlusion.
    bool target_inside = true;
};

/// \brief An axis-aligned rectangle in logical units.
using rect = centypo::rect;

/// \brief Width and height in logical units.
struct size
{
    float width = 0;
    float height = 0;
};

/// \brief A position or offset in logical units.
using vec2 = centypo::vec2;

struct safe_area_insets
{
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;

    bool operator==(const safe_area_insets&) const = default;
};

/// \brief Surface metrics in logical units, except viewport's framebuffer dimensions.
struct platform_state
{
    centui::viewport viewport;
    safe_area_insets safe_area;
    std::optional<rect> keyboard_occlusion;
    bool focused = true;
    bool active = true;
    bool drawable = true;
    bool closing = false;
    std::uint64_t surface_generation = 1;

    bool operator==(const platform_state&) const = default;
};

struct pointer_event
{
    input_event_type type = input_event_type::pointer_moved;
    pointer_id pointer = 0;
    pointer_device device = pointer_device::mouse;
    float x = 0;
    float y = 0;
    mouse_button button = mouse_button::primary;
    modifiers mods{};
    float delta_x = 0;
    float delta_y = 0;
    bool hover_capable = true;
    std::chrono::nanoseconds timestamp{};
};

struct key_event
{
    key code = key::unknown;
    bool pressed = true;
    modifiers mods{};
    bool repeat = false;
};

struct text_event
{
    std::string text;
};

struct composition_event
{
    std::string text;
    std::size_t selection_start = 0;
    std::size_t selection_length = 0;
};

struct focus_event
{
    bool focused;
};

enum class application_activity
{
    active,
    suspended,
    closing
};

struct lifecycle_event
{
    application_activity activity;
};

enum class surface_change
{
    metrics,
    unavailable,
    available,
    resources_invalidated
};

struct surface_event
{
    surface_change change;
    platform_state state;
};

using request_id = std::uint64_t;

enum class service_status
{
    success,
    cancelled,
    denied,
    failed,
    unsupported
};

struct service_result
{
    request_id request = 0;
    service_status status = service_status::success;
    std::string text;
};

using platform_event = std::variant<
    pointer_event,
    key_event,
    text_event,
    composition_event,
    focus_event,
    lifecycle_event,
    surface_event,
    service_result>;

[[nodiscard]] platform_event make_platform_event(const input_event& event);
[[nodiscard]] std::optional<input_event> make_input_event(const platform_event& event);

struct text_input_session
{
    rect area;
    bool multiline = false;

    bool operator==(const text_input_session&) const = default;
};

/// \brief Requests return IDs; completions are delivered through the platform event queue.
struct host_services
{
    std::function<request_id()> read_clipboard;
    std::function<request_id(std::string)> write_clipboard;
    std::function<void(std::optional<text_input_session>)> set_text_input;
};

enum class visibility
{
    visible,
    hidden,
    collapsed
};

enum class overflow
{
    visible,
    clip,
    scroll
};

enum class constraint_mode
{
    unconstrained,
    exact,
    maximum
};

/// \brief The available size and measurement mode along one axis.
struct axis_constraint
{
    float value = 0;
    constraint_mode mode = constraint_mode::unconstrained;
};

enum class unit
{
    px,
    percent,
    automatic,
    undefined,
};

/// \brief A logical length, percentage, automatic value, or absent constraint.
struct length
{
    float value = 0;
    centui::unit unit = centui::unit::automatic;

    constexpr bool operator==(const length&) const = default;
};

constexpr length px(float value)
{
    return {value, unit::px};
}

constexpr length percent(float value)
{
    return {value, unit::percent};
}

constexpr length automatic()
{
    return {0, unit::automatic};
}

/// \brief Clears a constraint or edge override; distinct from automatic sizing.
constexpr length undefined()
{
    return {0, unit::undefined};
}

/// \brief Main-axis sizing before and during flexible space distribution.
struct flex
{
    float grow = 0;
    float shrink = 0;
    length basis = automatic();

    static constexpr flex all(float grow, float shrink, length basis)
    {
        return {grow, shrink, basis};
    }

    constexpr bool operator==(const flex&) const = default;
};

/// \brief Physical and logical edges. Undefined entries allow axis fallbacks.
/// Logical start/end take precedence over left/right, then horizontal/vertical.
struct edges
{
    length top = undefined();

    length right = undefined();

    length bottom = undefined();

    length left = undefined();

    length start = undefined();
    length end = undefined();
    length horizontal = undefined();
    length vertical = undefined();

    static constexpr edges all(length value)
    {
        return {value, value, value, value};
    }

    constexpr bool operator==(const edges&) const = default;
};

enum class layout_direction
{
    inherit,
    ltr,
    rtl
};

enum class wrapping
{
    none,
    wrap,
    reverse
};

enum class box_sizing
{
    border_box,
    content_box
};

enum class display
{
    flex,
    none,
    contents
};

enum class position
{
    normal,
    relative,
    absolute
};

enum class direction
{
    column,
    column_reverse,
    row,
    row_reverse
};

enum class justification
{
    start,
    center,
    end,
    space_between,
    space_around,
    space_evenly
};

enum class alignment
{
    automatic,
    start,
    center,
    end,
    stretch,
    baseline,
    space_between,
    space_around,
    space_evenly
};

} // namespace centui
