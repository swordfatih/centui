
module;

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

import std;
import centui.node;
import centui.color;
import centui.unit;
import centui.draw;
import centui.glyph;
import centui.viewport;

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

class backend
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

    ~backend() = default;

    backend(const backend&) = delete;
    backend& operator=(const backend&) = delete;

    backend(centui::viewport&&, std::uint16_t = 0) = delete;
    backend(const centui::viewport&&, std::uint16_t = 0) = delete;

    [[nodiscard]] const centui::viewport& viewport() const noexcept
    {
        return _viewport;
    }

    [[nodiscard]] font& glyphs() noexcept
    {
        return _font;
    }

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
        ::bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
        ::bgfx::submit(_view_id, _program.get());
    }

    void text(const text_command& command)
    {
        if(command.color.a == 0 || command.value.empty())
        {
            return;
        }

        const auto fallback = command.shaped ? text_layout{} : _font.layout_text(command.value, command.font_size);
        const auto& shaped = command.shaped ? *command.shaped : fallback;
        const auto baseline = command.bounds.y + shaped.ascender;

        for(const auto& item: shaped.glyphs)
        {
            const auto& cached = get_glyph(item.id, command.font_size);

            rect bounds{
                .x = command.bounds.x + item.x + cached.left,
                .y = baseline - item.y - cached.top,
                .width = static_cast<float>(cached.width),
                .height = static_cast<float>(cached.height)};

            glyph(bounds, command.color, cached.texture.get());
        }
    }

    void draw(const node& root)
    {
        const auto list = commands(root);
        draw(list);
    }

    void draw(std::span<const draw_command> list)
    {
        if(_viewport.width <= 0 || _viewport.height <= 0)
        {
            return;
        }
        if(_viewport.width > std::numeric_limits<std::uint16_t>::max() || _viewport.height > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::invalid_argument("UI viewport exceeds bgfx view dimensions");
        }
        begin(static_cast<std::uint16_t>(_viewport.width), static_cast<std::uint16_t>(_viewport.height));
        for(const auto& command: list)
        {
            std::visit([&](const auto& item)
            {
                using type = std::decay_t<decltype(item)>;

                if constexpr(std::is_same_v<type, rectangle_command>)
                {
                    rectangle(item.bounds, item.color);
                }
                else
                {
                    text(item);
                }
            }, command);
        }
    }

private:
    struct cached_glyph
    {
        details::owned_handle<::bgfx::TextureHandle> texture{};
        int                                      width = 0;
        int                                      height = 0;
        int                                      left = 0;
        int                                      top = 0;
    };

    const cached_glyph& get_glyph(std::uint32_t id, float size)
    {
        const auto pixel_size = static_cast<std::uint32_t>(std::max(1.0f, std::round(size)));
        const auto key = (std::uint64_t(pixel_size) << 32) | id;

        if(const auto it = _glyphs.find(key); it != _glyphs.end())
        {
            return it->second;
        }

        const auto bitmap = _font.rasterize(id, size);

        cached_glyph entry{
            .width = bitmap.width,
            .height = bitmap.height,
            .left = bitmap.left,
            .top = bitmap.top};

        if(entry.width > 0 && entry.height > 0)
        {
            std::vector<std::uint32_t> pixels;
            pixels.reserve(bitmap.pixels.size());

            for(const auto alpha: bitmap.pixels)
            {
                pixels.push_back((std::uint32_t(alpha) << 24) | 0x00FFFFFF);
            }

            entry.texture = details::owned_handle{::bgfx::createTexture2D(
                static_cast<std::uint16_t>(entry.width),
                static_cast<std::uint16_t>(entry.height),
                false,
                1,
                ::bgfx::TextureFormat::BGRA8,
                BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
                ::bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size() * sizeof(std::uint32_t))))};

            if(!::bgfx::isValid(entry.texture.get()))
            {
                throw std::runtime_error("Failed to create glyph texture");
            }
        }

        return _glyphs.emplace(key, std::move(entry)).first->second;
    }

    void glyph(const rect& bounds, color value, ::bgfx::TextureHandle texture)
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

        v[0] = {bounds.x, bounds.y, 0.0f, 0.0f, 0.0f, packed};
        v[1] = {bounds.x + bounds.width, bounds.y, 0.0f, 1.0f, 0.0f, packed};
        v[2] = {bounds.x + bounds.width, bounds.y + bounds.height, 0.0f, 1.0f, 1.0f, packed};
        v[3] = {bounds.x, bounds.y + bounds.height, 0.0f, 0.0f, 1.0f, packed};

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
        ::bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
        ::bgfx::submit(_view_id, _text_program.get());
    }

    const centui::viewport& _viewport;
    font _font;
    std::uint16_t _view_id;

    ::bgfx::VertexLayout _vertex_layout;
    ::bgfx::VertexLayout _text_layout;

    details::owned_handle<::bgfx::ProgramHandle> _program;
    details::owned_handle<::bgfx::ProgramHandle> _text_program;
    details::owned_handle<::bgfx::UniformHandle> _sampler;

    std::unordered_map<std::uint64_t, cached_glyph> _glyphs;
};

} // namespace centui::bgfx
