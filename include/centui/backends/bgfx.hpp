#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <centui/platform.hpp>
#include <centui/render.hpp>
#include <centui/texture_cache.hpp>

#include <bgfx/bgfx.h>

namespace centui::details
{

// bgfx must remain initialized until every owned handle has been destroyed.
template <typename handle_type>
class owned_handle
{
public:
    explicit owned_handle(handle_type value = {std::numeric_limits<std::uint16_t>::max()})
        : m_value(value)
    {
    }

    ~owned_handle()
    {
        if(::bgfx::isValid(m_value))
        {
            ::bgfx::destroy(m_value);
        }
    }

    owned_handle(const owned_handle&) = delete;

    owned_handle& operator=(const owned_handle&) = delete;

    owned_handle(owned_handle&& other) noexcept
        : m_value(std::exchange(other.m_value, handle_type{std::numeric_limits<std::uint16_t>::max()}))
    {
    }

    owned_handle& operator=(owned_handle&& other) noexcept
    {
        std::swap(m_value, other.m_value);

        return *this;
    }

    [[nodiscard]] handle_type get() const noexcept
    {
        return m_value;
    }

private:
    handle_type m_value;
};

} // namespace centui::details

namespace centui::bgfx
{

/// \brief Renders draw lists with bgfx and owns cached GPU resources.
/// The platform or viewport passed to the constructor must outlive the backend.
class backend final : public centui::renderer
{
public:
    explicit backend(const centui::viewport& target, std::uint16_t view_id = 0);

    explicit backend(centui::platform& host, std::uint16_t view_id = 0);

    ~backend() override = default;

    backend(const backend&) = delete;

    backend& operator=(const backend&) = delete;

    backend(centui::viewport&&, std::uint16_t = 0) = delete;

    backend(const centui::viewport&&, std::uint16_t = 0) = delete;

    [[nodiscard]] centui::viewport viewport() const override;

    /// \brief Starts a frame; returns false when the surface has no drawable area.
    [[nodiscard]] bool begin_frame() override;

    [[nodiscard]] bool begin_frame(color clear);

    void end_frame() override;

    /// \brief Releases cached textures while retaining shader resources.
    void clear_cache();

    void set_cache_limit(std::size_t images);

    void set_cache_budget(std::size_t bytes);

    [[nodiscard]] centui::details::texture_cache_statistics cache_statistics() const;

    void draw(const draw_list& list) override;
    void on_surface_event(const surface_event& event) override;

private:
    struct context
    {
        context(render_surface target, centui::viewport size);

        ~context();

        context(const context&) = delete;

        context& operator=(const context&) = delete;

        // Retains named surface storage until after bgfx shuts down.
        render_surface target;
    };

    using texture_handle = details::owned_handle<::bgfx::TextureHandle>;
    using image_cache = centui::details::texture_cache<texture_handle>;

    static std::unique_ptr<context> create_context(centui::platform& host);

    backend(std::function<centui::viewport()> surface, std::unique_ptr<context> owner, std::uint16_t view_id);

    void begin(std::uint16_t width, std::uint16_t height, float scale);
    void initialize_resources();
    void release_resources();

    void submit(std::span<const draw_quad> commands);

    const texture_handle& get_glyph(const std::shared_ptr<const bitmap>& bitmap);

    // Declared first so bgfx outlives every resource destroyed below.
    std::unique_ptr<context> m_context;
    std::optional<std::reference_wrapper<centui::platform>> m_host;
    bool m_surface_available = true;
    std::function<centui::viewport()> m_surface;
    centui::viewport m_backbuffer;
    std::uint16_t m_view_id;

    ::bgfx::VertexLayout m_vertex_layout;

    details::owned_handle<::bgfx::ProgramHandle> m_program;
    details::owned_handle<::bgfx::ProgramHandle> m_text_program;
    details::owned_handle<::bgfx::UniformHandle> m_sampler;

    std::size_t m_cache_limit = 1024;
    std::size_t m_cache_budget = 32 * 1024 * 1024;
    image_cache m_glyphs;
};

} // namespace centui::bgfx
