#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <centui/platforms/sdl.hpp>
#include <centui/render.hpp>

#include <SDL3/SDL.h>

namespace centui::sdl::detail
{

struct renderer_deleter
{
    void operator()(SDL_Renderer* value) const noexcept;
};

struct texture_deleter
{
    void operator()(SDL_Texture* value) const noexcept;
};

} // namespace centui::sdl::detail

namespace centui::sdl
{

/// \brief Draws with an owned or borrowed SDL renderer and owns cached textures.
/// Borrowed targets and platform adapters must outlive the backend.
class backend final : public centui::renderer
{
public:
    explicit backend(platform& host);

    explicit backend(SDL_Renderer& renderer, float scale = 1);

    backend(SDL_Renderer& renderer, platform& host);

    backend(const backend&) = delete;

    backend& operator=(const backend&) = delete;

    backend(backend&&) = delete;

    backend& operator=(backend&&) = delete;

    void set_scale(float value);

    [[nodiscard]] centui::viewport viewport() const override;

    [[nodiscard]] bool begin_frame(color clear = {18, 24, 32, 255});

    void end_frame();

    void clear_cache();

    void set_cache_limit(std::size_t images);

    void draw(const draw_list& list) override;

private:
    using renderer_ptr = std::unique_ptr<SDL_Renderer, detail::renderer_deleter>;
    using texture_ptr = std::unique_ptr<SDL_Texture, detail::texture_deleter>;
    using image_cache = std::map<std::weak_ptr<const bitmap>, texture_ptr, std::owner_less<>>;

    static renderer_ptr create_renderer(platform& host);

    SDL_Texture& texture(const std::shared_ptr<const bitmap>& image);

    void quad(rect bounds, color tint, rect uv, std::optional<std::reference_wrapper<SDL_Texture>> image, float scale);

    // The borrowed renderer is either external or owned by m_owned.
    renderer_ptr m_owned;
    SDL_Renderer& m_renderer;
    std::optional<std::reference_wrapper<platform>> m_platform;
    float m_scale = 1;

    std::size_t m_cache_limit = 1024;
    image_cache m_images;
};

} // namespace centui::sdl
