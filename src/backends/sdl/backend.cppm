module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <SDL3/SDL.h>

export module centui.sdl_renderer;

import centui.render;
export import centui.sdl;

namespace centui::sdl::detail
{
struct renderer_deleter
{
    void operator()(SDL_Renderer* value) const noexcept
    {
        SDL_DestroyRenderer(value);
    }
};

struct texture_deleter
{
    void operator()(SDL_Texture* value) const noexcept
    {
        SDL_DestroyTexture(value);
    }
};
} // namespace centui::sdl::detail

export namespace centui::sdl
{
// A renderer borrowed from the application must outlive this backend.
class backend final : public centui::renderer
{
public:
    explicit backend(platform& host)
        : _owned(SDL_CreateRenderer(&host.native_window(), nullptr)), _renderer(_owned.get()), _platform(&host)
    {
        if(!_renderer)
        {
            throw std::runtime_error(SDL_GetError());
        }
    }

    explicit backend(SDL_Renderer& renderer, float scale = 1) : _renderer(&renderer)
    {
        set_scale(scale);
    }

    backend(SDL_Renderer& renderer, platform& host) : _renderer(&renderer), _platform(&host)
    {
        if(SDL_GetRenderWindow(&renderer) != &host.native_window())
        {
            throw std::invalid_argument("SDL renderer and platform must use the same window");
        }
    }

    backend(const backend&) = delete;
    backend& operator=(const backend&) = delete;
    backend(backend&&) = delete;
    backend& operator=(backend&&) = delete;

    void set_scale(float value)
    {
        if(!std::isfinite(value) || value <= 0)
        {
            throw std::invalid_argument("Display scale must be finite and positive");
        }
        _scale = value;
    }

    [[nodiscard]] centui::viewport viewport() const override
    {
        if(_platform)
        {
            return _platform->viewport();
        }
        int width = 0;
        int height = 0;
        if(!SDL_GetRenderOutputSize(_renderer, &width, &height))
        {
            throw std::runtime_error(SDL_GetError());
        }
        return {width, height, _scale};
    }

    [[nodiscard]] bool begin_frame(color clear = {18, 24, 32, 255})
    {
        const auto size = viewport();
        if(size.width <= 0 || size.height <= 0)
        {
            return false;
        }
        if(!SDL_SetRenderDrawColor(_renderer, clear.r, clear.g, clear.b, clear.a) || !SDL_RenderClear(_renderer))
        {
            throw std::runtime_error(SDL_GetError());
        }
        return true;
    }

    void end_frame()
    {
        if(!SDL_RenderPresent(_renderer))
        {
            throw std::runtime_error(SDL_GetError());
        }
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

        SDL_BlendMode previous_blend;
        if(!SDL_GetRenderDrawBlendMode(_renderer, &previous_blend) ||
           !SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND))
        {
            throw std::runtime_error(SDL_GetError());
        }
        struct restore_blend
        {
            SDL_Renderer* renderer;
            SDL_BlendMode mode;

            ~restore_blend()
            {
                SDL_SetRenderDrawBlendMode(renderer, mode);
            }
        } restore{_renderer, previous_blend};

        const rect clip{0, 0, size.logical_width(), size.logical_height()};
        for(const auto& command: list.commands)
        {
            std::visit([&](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<type, rectangle_command>)
                {
                    quad(intersection(intersection(item.bounds, item.clip), clip), item.color, {0, 0, 1, 1}, nullptr, size.scale);
                }
                else if constexpr(std::is_same_v<type, text_command>)
                {
                    details::for_each_glyph(item, clip, [&](const glyph_quad& glyph)
                    {
                        quad(glyph.bounds, glyph.color, glyph.uv, texture(glyph.image), size.scale);
                    });
                }
                else
                {
                    details::draw_image(item, clip, [&](rect bounds, rect uv)
                    {
                        quad(bounds, item.color, uv, texture(item.image), size.scale);
                    });
                }
            }, command);
        }
    }

private:
    SDL_Texture* texture(const std::shared_ptr<const bitmap>& image)
    {
        const std::weak_ptr<const bitmap> key{image};
        if(const auto found = _images.find(key); found != _images.end())
        {
            return found->second.get();
        }
        if(_images.size() >= _cache_limit)
        {
            clear_cache();
        }
        const auto pixels = rgba_pixels(*image);
        if(image->width > std::numeric_limits<int>::max() / 4)
        {
            throw std::length_error("Image row exceeds SDL texture pitch");
        }
        std::unique_ptr<SDL_Texture, detail::texture_deleter> result{
            SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image->width, image->height)};
        if(!result || !SDL_UpdateTexture(result.get(), nullptr, pixels.data(), image->width * 4) ||
           !SDL_SetTextureBlendMode(result.get(), SDL_BLENDMODE_BLEND) ||
           !SDL_SetTextureScaleMode(result.get(), SDL_SCALEMODE_NEAREST))
        {
            throw std::runtime_error(SDL_GetError());
        }
        return _images.emplace(key, std::move(result)).first->second.get();
    }

    void quad(rect bounds, color tint, rect uv, SDL_Texture* image, float scale)
    {
        if(tint.a == 0 || bounds.width <= 0 || bounds.height <= 0)
        {
            return;
        }
        const float right = (bounds.x + bounds.width) * scale;
        const float bottom = (bounds.y + bounds.height) * scale;
        const float left = bounds.x * scale;
        const float top = bounds.y * scale;
        const float u1 = uv.x + uv.width;
        const float v1 = uv.y + uv.height;
        const SDL_FColor color{tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, tint.a / 255.0f};
        const std::array vertices{
            SDL_Vertex{{left, top}, color, {uv.x, uv.y}},
            SDL_Vertex{{right, top}, color, {u1, uv.y}},
            SDL_Vertex{{right, bottom}, color, {u1, v1}},
            SDL_Vertex{{left, bottom}, color, {uv.x, v1}}};
        static constexpr std::array indices{0, 1, 2, 0, 2, 3};
        if(!SDL_RenderGeometry(_renderer, image, vertices.data(), static_cast<int>(vertices.size()),
                               indices.data(), static_cast<int>(indices.size())))
        {
            throw std::runtime_error(SDL_GetError());
        }
    }

    std::unique_ptr<SDL_Renderer, detail::renderer_deleter> _owned;
    SDL_Renderer* _renderer;
    platform* _platform = nullptr;
    float _scale = 1;
    std::size_t _cache_limit = 1024;
    std::map<std::weak_ptr<const bitmap>, std::unique_ptr<SDL_Texture, detail::texture_deleter>, std::owner_less<>> _images;
};
} // namespace centui::sdl
