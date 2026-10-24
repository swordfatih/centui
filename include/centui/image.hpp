#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include <centui/render.hpp>

namespace centui
{

enum class image_fit
{
    contain,
    cover,
    fill
};

struct image_options
{
    image_fit fit = image_fit::contain;
    image_sampling sampling = image_sampling::linear;

    constexpr bool operator==(const image_options&) const = default;
};

struct image_intrinsics
{
    std::optional<float> width;
    std::optional<float> height;
    std::optional<float> aspect_ratio;

    constexpr bool operator==(const image_intrinsics&) const = default;
};

class image_source
{
public:
    explicit image_source(bitmap value);

    [[nodiscard]] static image_source from_file(const std::filesystem::path& path);
    [[nodiscard]] static image_source from_bytes(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static image_source from_svg(std::string_view text);
    [[nodiscard]] static image_source from_bitmap(bitmap value);

    [[nodiscard]] image_intrinsics intrinsic() const;
    [[nodiscard]] std::shared_ptr<const void> identity() const;
    [[nodiscard]] std::shared_ptr<const bitmap> prepare(size logical_size, float scale = 1) const;

    void clear_cache() const;
    void set_cache_budget(std::size_t bytes) const;

private:
    struct data;
    explicit image_source(std::shared_ptr<data> value);
    [[nodiscard]] data& state() const;
    std::shared_ptr<data> m_data;
};

} // namespace centui
