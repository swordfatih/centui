#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <centui/render.hpp>

namespace centui
{

/// \brief Shapes and rasterizes text from a font file. Copies share the native font resources.
class font
{
public:
    /// \brief Loads a font file; throws when the file or native font resources cannot be opened.
    explicit font(const std::filesystem::path& path);

    ~font();

    font(const font&);

    font& operator=(const font&);

    font(font&&) noexcept;

    font& operator=(font&&) noexcept;

    [[nodiscard]] text_layout layout_text(std::string_view text, float size);

    [[nodiscard]] rect measure(std::string_view text, float size);

    [[nodiscard]] std::vector<shaped_glyph> shape(std::string_view text, float size);

    [[nodiscard]] glyph_bitmap rasterize(std::uint32_t id, float size);

    [[nodiscard]] float ascender(float size);

    [[nodiscard]] std::shared_ptr<const void> identity() const;

    bool operator==(const font&) const = default;

private:
    struct data;

    [[nodiscard]] data& state() const;

    std::shared_ptr<data> m_data;
};

/// \brief Prepares text layouts and caches glyph images at a configurable raster scale.
/// Copies share cached resources; each service keeps its own raster scale.
class text_service
{
public:
    text_service();

    /// \brief Sets pixels per logical unit. The scale must be finite and positive.
    void set_scale(float value);

    [[nodiscard]] float scale() const noexcept;

    explicit text_service(const centui::font& face);

    void set_font(const centui::font& face);

    [[nodiscard]] std::shared_ptr<const void> identity() const;

    [[nodiscard]] std::shared_ptr<const void> font_identity(
        const std::optional<centui::font>& face = std::nullopt
    ) const;

    /// \brief Releases cached glyph images. Existing prepared layouts remain usable.
    void clear_cache();

    /// \brief Sets the glyph cache budget in bytes and clears existing cache entries.
    void set_cache_budget(std::size_t bytes);

    // Newlines and space-delimited word wrapping. Long words overflow intact.
    // Shape selected lines again, preserving contextual shaping at line edges.
    [[nodiscard]] text_layout layout_text(
        std::string_view value,
        float size,
        float width = std::numeric_limits<float>::infinity(),
        const std::optional<centui::font>& face = std::nullopt
    ) const;

    // The caller may retain this layout across preparations and cache eviction.
    [[nodiscard]] std::shared_ptr<const text_layout> prepare(
        std::string_view value,
        const std::optional<centui::font>& face,
        float size,
        float width = std::numeric_limits<float>::infinity()
    ) const;

    [[nodiscard]] rect measure(std::string_view value, float size) const;

    /// \brief Adds physical glyph images to an existing logical layout without reshaping it.
    [[nodiscard]] std::shared_ptr<const text_layout> rasterize(
        const text_layout& layout,
        const std::optional<centui::font>& face,
        float size
    ) const;

private:
    struct data;

    const centui::font& resolve(const std::optional<centui::font>& face) const;

    std::shared_ptr<const glyph_bitmap> image(centui::font& face, std::uint32_t glyph, float size) const;

    std::shared_ptr<data> m_data;
    float m_scale = 1;
};

/// \brief Per-widget logical layout and paint cache. DPI changes only refresh glyph images.
class text_cache
{
public:
    [[nodiscard]] std::shared_ptr<const text_layout> layout(
        text_service& service,
        std::string_view value,
        const std::optional<centui::font>& face,
        float size,
        float width
    );

    [[nodiscard]] std::shared_ptr<const text_layout> rasterize(text_service& service);

    void reset();

private:
    std::string m_value;
    std::optional<centui::font> m_face;
    std::shared_ptr<const void> m_identity;
    float m_size = 0;
    float m_width = 0;
    std::shared_ptr<const text_layout> m_layout;
    std::shared_ptr<const text_layout> m_raster;
};

} // namespace centui
