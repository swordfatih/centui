module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <SFML/Graphics.hpp>

export module centui.sfml;

import centui.render;

export namespace centui::sfml
{

// Borrows the target. The host clears and presents; this renderer only draws.
class backend final : public centui::renderer
{
public:
    explicit backend(::sf::RenderTarget& target) : _target(target)
    {
    }

    ~backend() override = default;

    backend(const backend&) = delete;
    backend& operator=(const backend&) = delete;
    backend(backend&&) = delete;
    backend& operator=(backend&&) = delete;

    [[nodiscard]] centui::viewport viewport() const noexcept override
    {
        const auto size = _target.getSize();
        const auto maximum = static_cast<unsigned int>(std::numeric_limits<int>::max());
        return {static_cast<int>(std::min(size.x, maximum)), static_cast<int>(std::min(size.y, maximum))};
    }

    void clear_cache()
    {
        _images.clear();
    }

    void set_cache_limit(std::size_t images)
    {
        _cache_limit = std::max(std::size_t{1}, images);
        clear_cache();
    }

    void draw(const draw_list& list) override
    {
        const auto size = list.viewport;
        if(size.width <= 0 || size.height <= 0)
        {
            return;
        }
        std::erase_if(_images, [](const auto& entry)
        {
            return entry.first.expired();
        });

        struct restore_state
        {
            ::sf::RenderTarget& target;
            ::sf::View view;
            ~restore_state()
            {
                target.setView(view);
                target.popGLStates();
            }
        } restore{_target, _target.getView()};
        _target.pushGLStates();
        _target.setView(::sf::View{::sf::FloatRect{{0, 0}, {float(size.width), float(size.height)}}});
        const rect clip{0, 0, float(size.width), float(size.height)};
        for(const auto& command: list.commands)
        {
            std::visit([&](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<type, rectangle_command>)
                {
                    quad(intersection(intersection(item.bounds, item.clip), clip), item.color, {0, 0, 1, 1}, nullptr);
                }
                else
                {
                    details::for_each_glyph(item, clip, [&](const glyph_quad& glyph)
                    {
                        quad(glyph.bounds, glyph.color, glyph.uv, &texture(glyph.image));
                    });
                }
            }, command);
        }
    }

private:
    const ::sf::Texture& texture(const std::shared_ptr<const glyph_bitmap>& bitmap)
    {
        const std::weak_ptr<const glyph_bitmap> key{bitmap};
        if(const auto found = _images.find(key); found != _images.end())
        {
            return found->second;
        }
        if(_images.size() >= _cache_limit)
        {
            clear_cache();
        }
        if(bitmap->pixels.size() > std::numeric_limits<std::size_t>::max() / 4)
        {
            throw std::length_error("Glyph image exceeds SFML upload size");
        }
        std::vector<std::uint8_t> rgba(bitmap->pixels.size() * 4);
        for(std::size_t i = 0; i < bitmap->pixels.size(); ++i)
        {
            rgba[i * 4] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = bitmap->pixels[i];
        }
        const ::sf::Image image{{static_cast<unsigned int>(bitmap->width), static_cast<unsigned int>(bitmap->height)}, rgba.data()};
        ::sf::Texture texture;
        if(!texture.loadFromImage(image))
        {
            throw std::runtime_error("Failed to create SFML glyph texture");
        }
        texture.setSmooth(false);
        return _images.emplace(key, std::move(texture)).first->second;
    }

    void quad(rect bounds, color tint, rect uv, const ::sf::Texture* texture)
    {
        if(tint.a == 0 || bounds.width <= 0 || bounds.height <= 0)
        {
            return;
        }
        const auto size = texture ? texture->getSize() : ::sf::Vector2u{1, 1};
        const float u0 = uv.x * size.x;
        const float v0 = uv.y * size.y;
        const float u1 = (uv.x + uv.width) * size.x;
        const float v1 = (uv.y + uv.height) * size.y;
        const ::sf::Color color{tint.r, tint.g, tint.b, tint.a};
        const auto right = bounds.x + bounds.width;
        const auto bottom = bounds.y + bounds.height;
        const std::array vertices{
            ::sf::Vertex{{bounds.x, bounds.y}, color, {u0, v0}},
            ::sf::Vertex{{right, bounds.y}, color, {u1, v0}},
            ::sf::Vertex{{right, bottom}, color, {u1, v1}},
            ::sf::Vertex{{bounds.x, bounds.y}, color, {u0, v0}},
            ::sf::Vertex{{right, bottom}, color, {u1, v1}},
            ::sf::Vertex{{bounds.x, bottom}, color, {u0, v1}}};
        ::sf::RenderStates states;
        states.blendMode = ::sf::BlendAlpha;
        states.texture = texture;
        _target.draw(vertices.data(), vertices.size(), ::sf::PrimitiveType::Triangles, states);
    }

    ::sf::RenderTarget& _target;
    std::size_t _cache_limit = 1024;
    std::map<std::weak_ptr<const glyph_bitmap>, ::sf::Texture, std::owner_less<>> _images;
};

} // namespace centui::sfml
