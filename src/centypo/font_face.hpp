#pragma once

#include "layout_data.hpp"

#include <centypo/engine.hpp>

#include <optional>

namespace centypo::details
{

/// Retains exact font bytes and native configuration through shaping and rasterization.
class font_face
{
public:
    explicit font_face(
        std::shared_ptr<const std::vector<unsigned char>> bytes,
        long index = 0,
        std::optional<std::pair<int, font_slant>> configuration = {}
    );

    ~font_face();

    [[nodiscard]] std::string family() const;
    [[nodiscard]] int weight() const;
    [[nodiscard]] font_slant slant() const;
    [[nodiscard]] long face_count() const;

    [[nodiscard]] bool covers(std::uint32_t scalar) const;
    [[nodiscard]] bool variant(std::uint32_t scalar, std::uint32_t selector) const;
    [[nodiscard]] bool color() const;
    [[nodiscard]] bool variable() const;
    [[nodiscard]] long index() const;

    [[nodiscard]] std::size_t storage_bytes() const;
    [[nodiscard]] std::shared_ptr<const std::vector<unsigned char>> bytes() const;
    [[nodiscard]] std::shared_ptr<const void> identity() const;

    [[nodiscard]] layout_data shape_run(
        std::string_view text,
        float size,
        bool rtl,
        std::uint32_t script,
        std::string_view language,
        std::size_t begin = 0,
        std::size_t length = std::string_view::npos
    );

    [[nodiscard]] glyph_bitmap rasterize(std::uint32_t id, float size);

    [[nodiscard]] std::vector<float> ligature_carets(std::uint32_t glyph, float size, bool rtl);

private:
    struct data;

    [[nodiscard]] data& state() const;

    std::unique_ptr<data> m_data;
};

} // namespace centypo::details
