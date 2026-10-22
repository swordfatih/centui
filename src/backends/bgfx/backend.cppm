
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <essl/fs_color.sc.bin.h>
#include <essl/fs_text.sc.bin.h>
#include <essl/vs_color.sc.bin.h>
#include <essl/vs_text.sc.bin.h>
#include <glsl/fs_color.sc.bin.h>
#include <glsl/fs_text.sc.bin.h>
#include <glsl/vs_color.sc.bin.h>
#include <glsl/vs_text.sc.bin.h>
#include <spirv/fs_color.sc.bin.h>
#include <spirv/fs_text.sc.bin.h>
#include <spirv/vs_color.sc.bin.h>
#include <spirv/vs_text.sc.bin.h>

#if defined(_WIN32)
#include <dxbc/fs_color.sc.bin.h>
#include <dxbc/fs_text.sc.bin.h>
#include <dxbc/vs_color.sc.bin.h>
#include <dxbc/vs_text.sc.bin.h>
#endif

export module centui.bgfx;

import centui.render;

namespace centui::details
{

// bgfx must remain initialized until every owned handle has been destroyed.
template <typename handle_type>
class owned_handle
{
public:
    explicit owned_handle(handle_type value = {std::numeric_limits<std::uint16_t>::max()}) : _value(value)
    {
    }

    ~owned_handle()
    {
        if(::bgfx::isValid(_value))
        {
            ::bgfx::destroy(_value);
        }
    }

    owned_handle(const owned_handle&) = delete;
    owned_handle& operator=(const owned_handle&) = delete;

    owned_handle(owned_handle&& other) noexcept : _value(std::exchange(other._value, handle_type{std::numeric_limits<std::uint16_t>::max()}))
    {
    }

    owned_handle& operator=(owned_handle&& other) noexcept
    {
        std::swap(_value, other._value);
        return *this;
    }

    [[nodiscard]] handle_type get() const noexcept
    {
        return _value;
    }

private:
    handle_type _value;
};

::bgfx::ProgramHandle create_program(const ::bgfx::EmbeddedShader* shaders, const char* vertex, const char* fragment)
{
    const auto type = ::bgfx::getRendererType();
    owned_handle vs{::bgfx::createEmbeddedShader(shaders, type, vertex)};
    owned_handle fs{::bgfx::createEmbeddedShader(shaders, type, fragment)};

    if(!::bgfx::isValid(vs.get()) || !::bgfx::isValid(fs.get()))
    {
        throw std::runtime_error("Failed to create CentUI shaders");
    }

    const auto program = ::bgfx::createProgram(vs.get(), fs.get());

    if(!::bgfx::isValid(program))
    {
        throw std::runtime_error("Failed to create CentUI program");
    }

    return program;
}

struct color_vertex
{
    float         x;
    float         y;
    float         z;
    std::uint32_t rgba;
};

struct text_vertex
{
    float         x;
    float         y;
    float         z;
    float         u;
    float         v;
    std::uint32_t rgba;
};

static const ::bgfx::EmbeddedShader color_shaders[] = {
    {"vs_color",
     {
#if defined(_WIN32)
         {::bgfx::RendererType::Direct3D11, vs_color_dxbc, sizeof(vs_color_dxbc)},
#endif
         {::bgfx::RendererType::OpenGLES, vs_color_essl, sizeof(vs_color_essl)},
         {::bgfx::RendererType::OpenGL, vs_color_glsl, sizeof(vs_color_glsl)},
         {::bgfx::RendererType::Vulkan, vs_color_spv, sizeof(vs_color_spv)},
         {::bgfx::RendererType::Count, nullptr, 0}}},
    {"fs_color",
     {
#if defined(_WIN32)
         {::bgfx::RendererType::Direct3D11, fs_color_dxbc, sizeof(fs_color_dxbc)},
#endif
         {::bgfx::RendererType::OpenGLES, fs_color_essl, sizeof(fs_color_essl)},
         {::bgfx::RendererType::OpenGL, fs_color_glsl, sizeof(fs_color_glsl)},
         {::bgfx::RendererType::Vulkan, fs_color_spv, sizeof(fs_color_spv)},
         {::bgfx::RendererType::Count, nullptr, 0}}},
    BGFX_EMBEDDED_SHADER_END()};

static const ::bgfx::EmbeddedShader text_shaders[] = {
    {"vs_text",
     {
#if defined(_WIN32)
         {::bgfx::RendererType::Direct3D11, vs_text_dxbc, sizeof(vs_text_dxbc)},
#endif
         {::bgfx::RendererType::OpenGLES, vs_text_essl, sizeof(vs_text_essl)},
         {::bgfx::RendererType::OpenGL, vs_text_glsl, sizeof(vs_text_glsl)},
         {::bgfx::RendererType::Vulkan, vs_text_spv, sizeof(vs_text_spv)},
         {::bgfx::RendererType::Count, nullptr, 0}}},
    {"fs_text",
     {
#if defined(_WIN32)
         {::bgfx::RendererType::Direct3D11, fs_text_dxbc, sizeof(fs_text_dxbc)},
#endif
         {::bgfx::RendererType::OpenGLES, fs_text_essl, sizeof(fs_text_essl)},
         {::bgfx::RendererType::OpenGL, fs_text_glsl, sizeof(fs_text_glsl)},
         {::bgfx::RendererType::Vulkan, fs_text_spv, sizeof(fs_text_spv)},
         {::bgfx::RendererType::Count, nullptr, 0}}},
    BGFX_EMBEDDED_SHADER_END()};

std::uint32_t pack_color(color value)
{
    return std::uint32_t(value.r) | (std::uint32_t(value.g) << 8) | (std::uint32_t(value.b) << 16) | (std::uint32_t(value.a) << 24);
}

} // namespace centui::details

export namespace centui::bgfx
{

class backend final : public centui::renderer
{
public:
    explicit backend(const centui::viewport& target, std::uint16_t view_id = 0) : _viewport(target), _view_id(view_id)
    {
        _vertex_layout.begin()
            .add(::bgfx::Attrib::Position, 3, ::bgfx::AttribType::Float)
            .add(::bgfx::Attrib::Color0, 4, ::bgfx::AttribType::Uint8, true)
            .end();

        _text_layout.begin()
            .add(::bgfx::Attrib::Position, 3, ::bgfx::AttribType::Float)
            .add(::bgfx::Attrib::TexCoord0, 2, ::bgfx::AttribType::Float)
            .add(::bgfx::Attrib::Color0, 4, ::bgfx::AttribType::Uint8, true)
            .end();

        _program = details::owned_handle{details::create_program(details::color_shaders, "vs_color", "fs_color")};
        _text_program = details::owned_handle{details::create_program(details::text_shaders, "vs_text", "fs_text")};
        _sampler = details::owned_handle{::bgfx::createUniform("s_texColor", ::bgfx::UniformType::Sampler)};

        if(!::bgfx::isValid(_sampler.get()))
        {
            throw std::runtime_error("Failed to create CentUI text sampler");
        }
    }

    ~backend() override = default;

    backend(const backend&) = delete;
    backend& operator=(const backend&) = delete;

    backend(centui::viewport&&, std::uint16_t = 0) = delete;
    backend(const centui::viewport&&, std::uint16_t = 0) = delete;

    [[nodiscard]] centui::viewport viewport() const noexcept override
    {
        return _viewport;
    }

private:
    void begin(std::uint16_t width, std::uint16_t height)
    {
        ::bgfx::setViewRect(_view_id, 0, 0, width, height);
        ::bgfx::setViewMode(_view_id, ::bgfx::ViewMode::Sequential);

        std::array<float, 16> projection{};
        bx::mtxOrtho(projection.data(), 0.0f, float(width), float(height), 0.0f, 0.0f, 100.0f, 0.0f, ::bgfx::getCaps()->homogeneousDepth);

        ::bgfx::setViewTransform(_view_id, nullptr, projection.data());
    }

    void rectangle(const rect& bounds, color value)
    {
        if(value.a == 0 || bounds.width <= 0 || bounds.height <= 0)
        {
            return;
        }

        if(::bgfx::getAvailTransientVertexBuffer(4, _vertex_layout) < 4 || ::bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        ::bgfx::TransientVertexBuffer vertices;
        ::bgfx::TransientIndexBuffer  indices;

        ::bgfx::allocTransientVertexBuffer(&vertices, 4, _vertex_layout);
        ::bgfx::allocTransientIndexBuffer(&indices, 6);

        const auto packed = details::pack_color(value);
        auto v = std::span{reinterpret_cast<details::color_vertex*>(vertices.data), 4};

        v[0] = {bounds.x, bounds.y, 0.0f, packed};
        v[1] = {bounds.x + bounds.width, bounds.y, 0.0f, packed};
        v[2] = {bounds.x + bounds.width, bounds.y + bounds.height, 0.0f, packed};
        v[3] = {bounds.x, bounds.y + bounds.height, 0.0f, packed};

        auto i = std::span{reinterpret_cast<std::uint16_t*>(indices.data), 6};

        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
        i[3] = 0;
        i[4] = 2;
        i[5] = 3;

        ::bgfx::setVertexBuffer(0, &vertices);
        ::bgfx::setIndexBuffer(&indices);
        ::bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                         BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
                                                       BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
        ::bgfx::submit(_view_id, _program.get());
    }

public:
    void clear_cache()
    {
        _glyphs.clear();
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
        if(size.width > std::numeric_limits<std::uint16_t>::max() || size.height > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::invalid_argument("UI viewport exceeds bgfx view dimensions");
        }
        std::erase_if(_glyphs, [](const auto& entry)
        {
            return entry.first.expired();
        });
        begin(static_cast<std::uint16_t>(size.width), static_cast<std::uint16_t>(size.height));
        const rect clip{0, 0, float(size.width), float(size.height)};
        for(const auto& command: list.commands)
        {
            std::visit([&](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<type, rectangle_command>)
                {
                    rectangle(intersection(intersection(item.bounds, item.clip), clip), item.color);
                }
                else
                {
                    details::for_each_glyph(item, clip, [&](const glyph_quad& quad)
                    {
                        glyph(quad.bounds, quad.color, quad.uv, get_glyph(quad.image).get());
                    });
                }
            }, command);
        }
    }

private:
    const details::owned_handle<::bgfx::TextureHandle>& get_glyph(const std::shared_ptr<const glyph_bitmap>& bitmap)
    {
        const std::weak_ptr<const glyph_bitmap> key{bitmap};
        if(const auto found = _glyphs.find(key); found != _glyphs.end())
        {
            return found->second;
        }
        const auto limit = ::bgfx::getCaps()->limits.maxTextureSize;
        if(bitmap->width <= 0 || bitmap->height <= 0 ||
           std::cmp_greater(bitmap->width, limit) || std::cmp_greater(bitmap->height, limit) ||
           bitmap->width > std::numeric_limits<std::uint16_t>::max() ||
           bitmap->height > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::invalid_argument("Glyph image exceeds bgfx texture dimensions");
        }
        if(_glyphs.size() >= _cache_limit)
        {
            clear_cache();
        }
        if(bitmap->pixels.size() > std::numeric_limits<std::uint32_t>::max() / sizeof(std::uint32_t))
        {
            throw std::length_error("Glyph image exceeds bgfx upload size");
        }
        std::vector<std::uint32_t> pixels;
        pixels.reserve(bitmap->pixels.size());
        for(const auto alpha: bitmap->pixels)
        {
            pixels.push_back((std::uint32_t(alpha) << 24) | 0x00FFFFFF);
        }
        details::owned_handle texture{::bgfx::createTexture2D(
            static_cast<std::uint16_t>(bitmap->width), static_cast<std::uint16_t>(bitmap->height),
            false, 1, ::bgfx::TextureFormat::BGRA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
            ::bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size() * sizeof(std::uint32_t))))};
        if(!::bgfx::isValid(texture.get()))
        {
            throw std::runtime_error("Failed to create glyph texture");
        }
        return _glyphs.emplace(key, std::move(texture)).first->second;
    }
    void glyph(const rect& bounds, color value, rect uv, ::bgfx::TextureHandle texture)
    {
        if(!::bgfx::isValid(texture) || bounds.width <= 0 || bounds.height <= 0 || value.a == 0)
        {
            return;
        }

        if(::bgfx::getAvailTransientVertexBuffer(4, _text_layout) < 4 || ::bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        ::bgfx::TransientVertexBuffer vertices;
        ::bgfx::TransientIndexBuffer  indices;

        ::bgfx::allocTransientVertexBuffer(&vertices, 4, _text_layout);
        ::bgfx::allocTransientIndexBuffer(&indices, 6);

        const auto packed = details::pack_color(value);
        auto v = std::span{reinterpret_cast<details::text_vertex*>(vertices.data), 4};

        v[0] = {bounds.x, bounds.y, 0.0f, uv.x, uv.y, packed};
        v[1] = {bounds.x + bounds.width, bounds.y, 0.0f, uv.x + uv.width, uv.y, packed};
        v[2] = {bounds.x + bounds.width, bounds.y + bounds.height, 0.0f, uv.x + uv.width, uv.y + uv.height, packed};
        v[3] = {bounds.x, bounds.y + bounds.height, 0.0f, uv.x, uv.y + uv.height, packed};

        auto i = std::span{reinterpret_cast<std::uint16_t*>(indices.data), 6};

        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
        i[3] = 0;
        i[4] = 2;
        i[5] = 3;

        ::bgfx::setVertexBuffer(0, &vertices);
        ::bgfx::setIndexBuffer(&indices);
        ::bgfx::setTexture(0, _sampler.get(), texture);
        ::bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                         BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
                                                       BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
        ::bgfx::submit(_view_id, _text_program.get());
    }

    const centui::viewport& _viewport;
    std::uint16_t _view_id;

    ::bgfx::VertexLayout _vertex_layout;
    ::bgfx::VertexLayout _text_layout;

    details::owned_handle<::bgfx::ProgramHandle> _program;
    details::owned_handle<::bgfx::ProgramHandle> _text_program;
    details::owned_handle<::bgfx::UniformHandle> _sampler;

    std::size_t _cache_limit = 1024;
    std::map<std::weak_ptr<const glyph_bitmap>, details::owned_handle<::bgfx::TextureHandle>, std::owner_less<>> _glyphs;
};

} // namespace centui::bgfx

