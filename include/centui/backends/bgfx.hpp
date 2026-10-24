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

    template <typename Platform>
        requires std::derived_from<Platform, centui::platform> && requires(Platform& host) { host.native_handles(); }
    explicit backend(Platform& host, std::uint16_t view_id = 0)
        : backend(
              [&host]
              {
                  return host.viewport();
              },
              create_context(host.native_handles(), host.viewport()),
              view_id
          )
    {
    }

    ~backend() override = default;

    backend(const backend&) = delete;

    backend& operator=(const backend&) = delete;

    backend(centui::viewport&&, std::uint16_t = 0) = delete;

    backend(const centui::viewport&&, std::uint16_t = 0) = delete;

    [[nodiscard]] centui::viewport viewport() const override;

    /// \brief Starts a frame; returns false when the surface has no drawable area.
    [[nodiscard]] bool begin_frame(color clear = {32, 40, 64, 255});

    void end_frame();

    /// \brief Releases cached textures while retaining shader resources.
    void clear_cache();

    void set_cache_limit(std::size_t images);

    void draw(const draw_list& list) override;

private:
    struct context
    {
        explicit context(const ::bgfx::Init& init);

        ~context();

        context(const context&) = delete;

        context& operator=(const context&) = delete;
    };

    using texture_handle = details::owned_handle<::bgfx::TextureHandle>;
    using image_cache = std::map<std::weak_ptr<const bitmap>, texture_handle, std::owner_less<>>;

    static std::unique_ptr<context> create_context(native_window_handles handles, centui::viewport size);

    backend(std::function<centui::viewport()> surface, std::unique_ptr<context> owner, std::uint16_t view_id);

    void begin(std::uint16_t width, std::uint16_t height, float scale);

    void rectangle(const rect& bounds, color value);

    const texture_handle& get_glyph(const std::shared_ptr<const bitmap>& bitmap);

    void glyph(
        const rect& bounds,
        color value,
        rect uv,
        ::bgfx::TextureHandle texture,
        image_sampling sampling = image_sampling::nearest
    );

    // Declared first so bgfx outlives every resource destroyed below.
    std::unique_ptr<context> m_context;
    std::function<centui::viewport()> m_surface;
    centui::viewport m_backbuffer;
    std::uint16_t m_view_id;

    ::bgfx::VertexLayout m_vertex_layout;
    ::bgfx::VertexLayout m_text_layout;

    details::owned_handle<::bgfx::ProgramHandle> m_program;
    details::owned_handle<::bgfx::ProgramHandle> m_text_program;
    details::owned_handle<::bgfx::UniformHandle> m_sampler;

    std::size_t m_cache_limit = 1024;
    image_cache m_glyphs;
};

} // namespace centui::bgfx
