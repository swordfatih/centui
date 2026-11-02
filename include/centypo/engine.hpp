#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <centypo/layout.hpp>
#include <centypo/unicode.hpp>

namespace centypo
{

enum class text_wrap
{
    word,
    grapheme,
    word_grapheme,
    none
};
enum class text_direction
{
    automatic,
    ltr,
    rtl
};

/// \brief Paragraph layout in logical units. Stops must be positive and strictly increasing.
struct text_options
{
    text_wrap wrap = text_wrap::word;
    text_direction direction = text_direction::automatic;
    std::string language;
    std::vector<float> tab_stops;
    float line_spacing = 0;
    std::size_t max_lines = 0;
    bool ellipsis = false;

    void validate() const;

    bool operator==(const text_options&) const = default;
};

enum class font_slant
{
    normal,
    italic,
    oblique
};

/// Ordered family request; the engine owns loaded bytes and native faces.
struct font
{
    font() = default;

    template <typename Family, typename... Families>
        requires std::constructible_from<std::string, Family&&> &&
                 (std::constructible_from<std::string, Families &&> && ...)
    explicit font(Family&& family, Families&&... fallback)
        : families{std::string(std::forward<Family>(family)), std::string(std::forward<Families>(fallback))...}
    {
    }

    explicit font(std::vector<std::string> families, int weight = 400, font_slant slant = font_slant::normal);

    std::vector<std::string> families = {"sans-serif"};
    int weight = 400;
    font_slant slant = font_slant::normal;

    void validate() const;

    bool operator==(const font&) const = default;
};

struct text_cache_budgets
{
    std::size_t shaping = 4 * 1024 * 1024;
    std::size_t layouts = 4 * 1024 * 1024;
    std::size_t images = 8 * 1024 * 1024;
};

struct text_cache_statistics
{
    std::size_t shaping_bytes = 0;
    std::size_t layout_bytes = 0;
    std::size_t image_bytes = 0;

    std::size_t retained_bytes = 0;
    std::size_t temporary_bytes = 0;
    std::size_t font_bytes = 0;

    std::size_t shapes = 0;
    std::size_t shaping_hits = 0;
    std::size_t layout_hits = 0;
};

struct engine_options
{
    bool system_fonts = true;
    text_cache_budgets caches{};
};

struct layout_options
{
    float size = 16;
    float width = std::numeric_limits<float>::infinity();

    centypo::font font{};
    text_options paragraph{};
    text_alignment alignment = text_alignment::left;
};

/// Owns font discovery, application fonts and bounded text caches.
/// Use on one application thread. Retained layouts are independent immutable values.
class engine
{
public:
    explicit engine(engine_options options = {});

    ~engine();

    engine(engine&&) noexcept;
    engine& operator=(engine&&) noexcept;

    engine(const engine&) = delete;
    engine& operator=(const engine&) = delete;

    [[nodiscard]] centypo::layout layout(std::string_view text, const layout_options& options = {}) const;

    /// Reuses prepared storage for the same immutable layout and scale, within cache budgets.
    /// Rasterizing an already prepared layout at its current scale returns that value unchanged.
    [[nodiscard]] centypo::layout rasterize(const centypo::layout& text, float scale = 1) const;

    void register_font(const std::filesystem::path& path);
    void register_font(std::span<const std::byte> bytes);

    void set_font_alias(std::string alias, std::vector<std::string> families);
    void refresh_fonts();
    void set_system_font_discovery(bool enabled);

    void clear_cache();
    void set_cache_budgets(text_cache_budgets budgets);

    [[nodiscard]] text_cache_statistics statistics() const;

    /// Generation counter incremented when a replacement font collection is published.
    [[nodiscard]] std::uint64_t revision() const;

private:
    struct data;

    [[nodiscard]] data& state() const;

    std::unique_ptr<data> m_data;
};

} // namespace centypo
