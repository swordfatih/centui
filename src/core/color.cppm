export module centui.color;

import std;

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
