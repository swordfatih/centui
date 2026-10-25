#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <centui/platforms/sfml.hpp>
#include <centui/render.hpp>
#include <centui/texture_cache.hpp>

#include <SFML/Graphics.hpp>

namespace centui::sfml
{

/// \brief Draws into a borrowed SFML render target and owns cached textures.
/// Borrowed targets and platform adapters must outlive the backend.
class backend final : public centui::renderer
{
public:
    explicit backend(::sf::RenderWindow& window);

    explicit backend(platform& host);

    explicit backend(::sf::RenderTarget& target, float scale);

    void set_scale(float value);

    ~backend() override = default;

    backend(const backend&) = delete;

    backend& operator=(const backend&) = delete;

    backend(backend&&) = delete;

    backend& operator=(backend&&) = delete;

    [[nodiscard]] centui::viewport viewport() const override;

    [[nodiscard]] bool begin_frame() override;

    [[nodiscard]] bool begin_frame(color clear);

    void end_frame() override;

    void clear_cache();

    void set_cache_limit(std::size_t images);

    void set_cache_budget(std::size_t bytes);

    [[nodiscard]] centui::details::texture_cache_statistics cache_statistics() const;

    void draw(const draw_list& list) override;

private:
    const ::sf::Texture& texture(
        const std::shared_ptr<const bitmap>& bitmap,
        image_sampling sampling = image_sampling::nearest
    );

    void quad(rect bounds, color tint, rect uv, std::optional<std::reference_wrapper<const ::sf::Texture>> texture);

    ::sf::RenderTarget& m_target;
    std::optional<std::reference_wrapper<::sf::RenderWindow>> m_window;
    std::optional<std::reference_wrapper<platform>> m_platform;
    float m_scale = 1;
    std::size_t m_cache_limit = 1024;
    std::size_t m_cache_budget = 32 * 1024 * 1024;
    centui::details::texture_cache<::sf::Texture> m_images;
};

} // namespace centui::sfml
