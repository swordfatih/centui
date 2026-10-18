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

} // namespace centui
