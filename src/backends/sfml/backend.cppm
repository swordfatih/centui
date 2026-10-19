module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <SFML/Graphics.hpp>

export module centui.sfml;

import centui.draw;
import centui.glyph;
import centui.color;
import centui.unit;
import centui.viewport;

export namespace centui::sfml
{

// Borrows the host's target. Drawing never clears it or presents its frame.
class backend
{
public:
    explicit backend(::sf::RenderTarget& target) : _target(target)
    {
    }

    backend(const backend&) = delete;
    backend& operator=(const backend&) = delete;
    backend(backend&&) = delete;
    backend& operator=(backend&&) = delete;

    [[nodiscard]] centui::viewport viewport() const
    {
        const auto size = _target.getSize();
        if(size.x > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
           size.y > static_cast<unsigned int>(std::numeric_limits<int>::max()))
        {
            throw std::overflow_error("SFML render target exceeds CentUI viewport dimensions");
        }

        return {static_cast<int>(size.x), static_cast<int>(size.y)};
    }

    [[nodiscard]] centui::font& glyphs() noexcept
    {
        return _font;
    }

    void draw(std::span<const centui::draw_command> commands)
    {
        for(const auto& command: commands)
        {
            std::visit([&](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<type, centui::rectangle_command>)
                {
                    draw_rectangle(item);
                }
                else
                {
                    draw_text(item);
                }
            }, command);
        }
    }

private:
    struct cached_glyph
    {
        ::sf::Texture texture;
        int width = 0;
        int height = 0;
        int left = 0;
        int top = 0;
    };

    static ::sf::Color to_sfml(centui::color value)
    {
        return {value.r, value.g, value.b, value.a};
    }

    void draw_rectangle(const centui::rectangle_command& command)
    {
        const auto& bounds = command.bounds;
        if(command.color.a == 0 || bounds.width <= 0 || bounds.height <= 0)
        {
            return;
        }

        const auto color = to_sfml(command.color);
        const auto right = bounds.x + bounds.width;
        const auto bottom = bounds.y + bounds.height;
        const std::array vertices{
            ::sf::Vertex{{bounds.x, bounds.y}, color},
            ::sf::Vertex{{right, bounds.y}, color},
            ::sf::Vertex{{right, bottom}, color},
            ::sf::Vertex{{bounds.x, bounds.y}, color},
            ::sf::Vertex{{right, bottom}, color},
            ::sf::Vertex{{bounds.x, bottom}, color}};

        _target.draw(vertices.data(), vertices.size(), ::sf::PrimitiveType::Triangles);
    }

    void draw_text(const centui::text_command& command)
    {
        if(command.color.a == 0 || command.value.empty())
        {
            return;
        }

        const auto fallback = command.shaped ? centui::text_layout{} : _font.layout_text(command.value, command.font_size);
        const auto& shaped = command.shaped ? command.shaped.value() : fallback;
        const auto baseline = command.bounds.y + shaped.ascender;

        for(const auto& item: shaped.glyphs)
        {
            const auto& glyph = get_glyph(item.id, command.font_size);
            const centui::rect bounds{
                .x = command.bounds.x + item.x + glyph.left,
                .y = baseline - item.y - glyph.top,
                .width = static_cast<float>(glyph.width),
                .height = static_cast<float>(glyph.height)};
            draw_glyph(bounds, command.color, glyph.texture);
        }
    }

    const cached_glyph& get_glyph(std::uint32_t id, float size)
    {
        const auto pixel_size = static_cast<std::uint32_t>(std::max(1.0f, std::round(size)));
        const auto key = (std::uint64_t(pixel_size) << 32) | id;
        if(const auto it = _glyphs.find(key); it != _glyphs.end())
        {
            return it->second;
        }

        const auto bitmap = _font.rasterize(id, size);
        cached_glyph glyph{};
        glyph.width = bitmap.width;
        glyph.height = bitmap.height;
        glyph.left = bitmap.left;
        glyph.top = bitmap.top;

        if(glyph.width > 0 && glyph.height > 0)
        {
            std::vector<std::uint8_t> rgba(bitmap.pixels.size() * 4);
            for(std::size_t i = 0; i < bitmap.pixels.size(); ++i)
            {
                rgba[i * 4] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = bitmap.pixels[i];
            }

            const ::sf::Image image{
                {static_cast<unsigned int>(glyph.width), static_cast<unsigned int>(glyph.height)},
                rgba.data()};
            if(!glyph.texture.loadFromImage(image))
            {
                throw std::runtime_error("Failed to create SFML glyph texture");
            }
            glyph.texture.setSmooth(false);
        }

        return _glyphs.emplace(key, std::move(glyph)).first->second;
    }

    void draw_glyph(const centui::rect& bounds, centui::color tint, const ::sf::Texture& texture)
    {
        if(bounds.width <= 0 || bounds.height <= 0)
        {
            return;
        }

        const auto color = to_sfml(tint);
        const auto right = bounds.x + bounds.width;
        const auto bottom = bounds.y + bounds.height;
        const auto texture_size = texture.getSize();
        const auto u = static_cast<float>(texture_size.x);
        const auto v = static_cast<float>(texture_size.y);
        const std::array vertices{
            ::sf::Vertex{{bounds.x, bounds.y}, color, {0.0f, 0.0f}},
            ::sf::Vertex{{right, bounds.y}, color, {u, 0.0f}},
            ::sf::Vertex{{right, bottom}, color, {u, v}},
            ::sf::Vertex{{bounds.x, bounds.y}, color, {0.0f, 0.0f}},
            ::sf::Vertex{{right, bottom}, color, {u, v}},
            ::sf::Vertex{{bounds.x, bottom}, color, {0.0f, v}}};

        ::sf::RenderStates states;
        states.blendMode = ::sf::BlendAlpha;
        states.texture = &texture;
        _target.draw(vertices.data(), vertices.size(), ::sf::PrimitiveType::Triangles, states);
    }

    ::sf::RenderTarget& _target;
    centui::font _font;
    std::unordered_map<std::uint64_t, cached_glyph> _glyphs;
};

} // namespace centui::sfml
