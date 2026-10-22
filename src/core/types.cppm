module;

#include <cstdint>

export module centui.types;

export namespace centui
{

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
            255};
    }

    constexpr bool operator==(const color&) const = default;
};

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

} // namespace centui

export namespace centui
{

struct viewport
{
    int width = 0;
    int height = 0;
};

enum class input_event_type
{
    pointer_moved,
    pointer_pressed,
    pointer_released,
    pointer_left,
    pointer_cancelled,
    focus_lost,
    focus_gained
};

enum class mouse_button
{
    primary,
    secondary,
    middle,
    other
};

struct input_event
{
    input_event_type type;
    float x = 0;
    float y = 0;
    mouse_button button = mouse_button::primary;
};

struct rect
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    constexpr bool operator==(const rect&) const = default;
};

struct size
{
    float width = 0;
    float height = 0;
};

struct vec2
{
    float x = 0;
    float y = 0;
    constexpr bool operator==(const vec2&) const = default;
};

enum class constraint_mode
{
    unconstrained,
    exact,
    maximum
};

struct axis_constraint
{
    float value = 0;
    constraint_mode mode = constraint_mode::unconstrained;
};

enum class unit
{
    px,
    percent,
    fit,
};

struct length
{
    float value = 0;
    unit  unit = unit::fit;

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

constexpr length fit()
{
    return {0, unit::fit};
}

struct edges
{
    length top = fit();
    length right = fit();
    length bottom = fit();
    length left = fit();

    static constexpr edges all(length value)
    {
        return {value, value, value, value};
    }

    constexpr bool operator==(const edges&) const = default;
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

enum class text_alignment
{
    left,
    center,
    right
};

} // namespace centui

export namespace centui
{
// Zero selects the application's default font. Other IDs belong to a text service.
struct font_handle
{
    std::uint64_t value = 0;
    constexpr bool operator==(const font_handle&) const = default;
};

}
