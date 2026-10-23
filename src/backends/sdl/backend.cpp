#include <centui/backends/sdl.hpp>

namespace centui::sdl::detail
{

void renderer_deleter::operator()(SDL_Renderer* value) const noexcept
{
    SDL_DestroyRenderer(value);
}

void texture_deleter::operator()(SDL_Texture* value) const noexcept
{
    SDL_DestroyTexture(value);
}

} // namespace centui::sdl::detail

namespace centui::sdl
{

backend::renderer_ptr backend::create_renderer(platform& host)
{
    renderer_ptr renderer{SDL_CreateRenderer(&host.native_window(), nullptr)};

    if(!renderer)
    {
        throw std::runtime_error(SDL_GetError());
    }

    return renderer;
}

backend::backend(platform& host)
    : m_owned(create_renderer(host)),
      m_renderer(*m_owned),
      m_platform(host)
{
}

backend::backend(SDL_Renderer& renderer, float scale)
    : m_renderer(renderer)
{
    set_scale(scale);
}

backend::backend(SDL_Renderer& renderer, platform& host)
    : m_renderer(renderer),
      m_platform(host)
{
    if(SDL_GetRenderWindow(&renderer) != &host.native_window())
    {
        throw std::invalid_argument("SDL renderer and platform must use the same window");
    }
}

void backend::set_scale(float value)
{
    if(!std::isfinite(value) || value <= 0)
    {
        throw std::invalid_argument("Display scale must be finite and positive");
    }

    m_scale = value;
}

centui::viewport backend::viewport() const
{
    if(m_platform)
    {
        return m_platform->get().viewport();
    }

    int width = 0;
    int height = 0;

    if(!SDL_GetRenderOutputSize(&m_renderer, &width, &height))
    {
        throw std::runtime_error(SDL_GetError());
    }

    return {width, height, m_scale};
}

bool backend::begin_frame(color clear)
{
    const auto size = viewport();

    if(size.width <= 0 || size.height <= 0)
    {
        return false;
    }

    if(!SDL_SetRenderDrawColor(&m_renderer, clear.r, clear.g, clear.b, clear.a) || !SDL_RenderClear(&m_renderer))
    {
        throw std::runtime_error(SDL_GetError());
    }

    return true;
}

void backend::end_frame()
{
    if(!SDL_RenderPresent(&m_renderer))
    {
        throw std::runtime_error(SDL_GetError());
    }
}

void backend::clear_cache()
{
    m_images.clear();
}

void backend::set_cache_limit(std::size_t images)
{
    m_cache_limit = std::max(std::size_t{1}, images);
    clear_cache();
}

void backend::draw(const draw_list& list)
{
    const auto size = list.viewport;

    if(size.width <= 0 || size.height <= 0)
    {
        return;
    }

    std::erase_if(
        m_images,
        [](const auto& entry)
        {
            return entry.first.expired();
        }
    );

    SDL_BlendMode previous_blend;

    if(!SDL_GetRenderDrawBlendMode(&m_renderer, &previous_blend) ||
       !SDL_SetRenderDrawBlendMode(&m_renderer, SDL_BLENDMODE_BLEND))
    {
        throw std::runtime_error(SDL_GetError());
    }

    struct restore_blend
    {
        SDL_Renderer& renderer;
        SDL_BlendMode mode;

        ~restore_blend()
        {
            SDL_SetRenderDrawBlendMode(&renderer, mode);
        }
    } restore{m_renderer, previous_blend};

    const rect clip{0, 0, size.logical_width(), size.logical_height()};

    for(const auto& command: list.commands)
    {
        std::visit(
            [&](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;

                if constexpr(std::is_same_v<type, rectangle_command>)
                {
                    quad(
                        intersection(intersection(item.bounds, item.clip), clip),
                        item.color,
                        {0, 0, 1, 1},
                        std::nullopt,
                        size.scale
                    );
                }
                else if constexpr(std::is_same_v<type, text_command>)
                {
                    details::for_each_glyph(
                        item,
                        clip,
                        [&](const glyph_quad& glyph)
                        {
                            quad(glyph.bounds, glyph.color, glyph.uv, texture(glyph.image), size.scale);
                        }
                    );
                }
                else
                {
                    details::draw_image(
                        item,
                        clip,
                        [&](rect bounds, rect uv)
                        {
                            quad(bounds, item.color, uv, texture(item.image), size.scale);
                        }
                    );
                }
            },
            command
        );
    }
}

SDL_Texture& backend::texture(const std::shared_ptr<const bitmap>& image)
{
    const std::weak_ptr<const bitmap> key{image};

    if(const auto found = m_images.find(key); found != m_images.end())
    {
        return *found->second;
    }

    if(m_images.size() >= m_cache_limit)
    {
        clear_cache();
    }

    const auto pixels = rgba_pixels(*image);

    if(image->width > std::numeric_limits<int>::max() / 4)
    {
        throw std::length_error("Image row exceeds SDL texture pitch");
    }

    std::unique_ptr<SDL_Texture, detail::texture_deleter> result{
        SDL_CreateTexture(&m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image->width, image->height)
    };

    if(!result || !SDL_UpdateTexture(result.get(), nullptr, pixels.data(), image->width * 4) ||
       !SDL_SetTextureBlendMode(result.get(), SDL_BLENDMODE_BLEND) ||
       !SDL_SetTextureScaleMode(result.get(), SDL_SCALEMODE_NEAREST))
    {
        throw std::runtime_error(SDL_GetError());
    }

    return *m_images.emplace(key, std::move(result)).first->second;
}

void backend::quad(
    rect bounds,
    color tint,
    rect uv,
    std::optional<std::reference_wrapper<SDL_Texture>> image,
    float scale
)
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
        SDL_Vertex{{left, bottom}, color, {uv.x, v1}}
    };
    static constexpr std::array indices{0, 1, 2, 0, 2, 3};

    if(!SDL_RenderGeometry(
           &m_renderer,
           image ? &image->get() : nullptr,
           vertices.data(),
           static_cast<int>(vertices.size()),
           indices.data(),
           static_cast<int>(indices.size())
       ))
    {
        throw std::runtime_error(SDL_GetError());
    }
}

} // namespace centui::sdl
