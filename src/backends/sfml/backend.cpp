#include <centui/backends/sfml.hpp>

namespace centui::sfml
{

backend::backend(::sf::RenderWindow& window)
    : m_target(window),
      m_window(window)
{
}

backend::backend(platform& host)
    : backend(host.native_window())
{
    m_platform = host;
}

backend::backend(::sf::RenderTarget& target, float scale)
    : m_target(target)
{
    set_scale(scale);
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

    const auto size = m_target.getSize();
    const auto maximum = static_cast<unsigned int>(std::numeric_limits<int>::max());

    return {static_cast<int>(std::min(size.x, maximum)), static_cast<int>(std::min(size.y, maximum)), m_scale};
}

bool backend::begin_frame()
{
    return begin_frame({18, 24, 32, 255});
}

bool backend::begin_frame(color clear)
{
    if(!m_window)
    {
        throw std::logic_error("Frame presentation requires a window-backed renderer");
    }

    if(!m_window->get().isOpen())
    {
        return false;
    }

    m_window->get().clear({clear.r, clear.g, clear.b, clear.a});

    return true;
}

void backend::end_frame()
{
    if(!m_window)
    {
        throw std::logic_error("Frame presentation requires a window-backed renderer");
    }

    m_window->get().display();
}

void backend::set_cache_budget(std::size_t bytes)
{
    m_cache_budget = bytes;
}

centui::details::texture_cache_statistics backend::cache_statistics() const
{
    return m_images.statistics();
}

void backend::clear_cache()
{
    m_images.clear();
}

void backend::set_cache_limit(std::size_t images)
{
    m_cache_limit = std::max(std::size_t{1}, images);
}

void backend::draw(const draw_list& list)
{
    const auto size = list.viewport;

    if(size.width <= 0 || size.height <= 0)
    {
        return;
    }

    m_images.begin_frame(list, m_cache_limit, m_cache_budget);

    struct restore_state
    {
        ::sf::RenderTarget& target;
        ::sf::View view;

        ~restore_state()
        {
            target.setView(view);
            target.popGLStates();
        }
    } restore{m_target, m_target.getView()};

    m_target.pushGLStates();
    m_target.setView(::sf::View{::sf::FloatRect{{0, 0}, {size.logical_width(), size.logical_height()}}});
    for(const auto& item: list.commands)
    {
        quad(
            item.bounds,
            item.color,
            item.uv,
            item.image ? std::optional{std::cref(texture(item.image, item.sampling))} : std::nullopt
        );
    }
}

const ::sf::Texture& backend::texture(const std::shared_ptr<const bitmap>& bitmap, image_sampling sampling)
{
    const std::weak_ptr<const centui::bitmap> key{bitmap};

    if(const auto found = m_images.find(key); found != m_images.end())
    {
        found->second.setSmooth(sampling == image_sampling::linear);
        return found->second;
    }

    const auto rgba = rgba_pixels(*bitmap);
    const ::sf::Image image{
        {static_cast<unsigned int>(bitmap->width), static_cast<unsigned int>(bitmap->height)},
        rgba.data()
    };
    ::sf::Texture texture;

    if(!texture.loadFromImage(image))
    {
        throw std::runtime_error("Failed to create SFML glyph texture");
    }

    texture.setSmooth(sampling == image_sampling::linear);

    return m_images.emplace(key, std::move(texture)).first->second;
}

void backend::quad(rect bounds, color tint, rect uv, std::optional<std::reference_wrapper<const ::sf::Texture>> texture)
{
    if(tint.a == 0 || bounds.width <= 0 || bounds.height <= 0)
    {
        return;
    }

    const auto size = texture ? texture->get().getSize() : ::sf::Vector2u{1, 1};
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
        ::sf::Vertex{{bounds.x, bottom}, color, {u0, v1}}
    };
    ::sf::RenderStates states;
    states.blendMode = ::sf::BlendAlpha;
    states.texture = texture ? std::addressof(texture->get()) : nullptr;
    m_target.draw(vertices.data(), vertices.size(), ::sf::PrimitiveType::Triangles, states);
}

} // namespace centui::sfml
