
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

export module centui.renderer;

import std;
import centui.node;
import centui.color;
import centui.unit;
import centui.draw;
import centui.font;

namespace centui::details
{

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

static const bgfx::EmbeddedShader color_shaders[] = {
    {"vs_color",
     {
#if defined(_WIN32)
         {bgfx::RendererType::Direct3D11, vs_color_dxbc, sizeof(vs_color_dxbc)},
#endif
         {bgfx::RendererType::OpenGLES, vs_color_essl, sizeof(vs_color_essl)},
         {bgfx::RendererType::OpenGL, vs_color_glsl, sizeof(vs_color_glsl)},
         {bgfx::RendererType::Vulkan, vs_color_spv, sizeof(vs_color_spv)},
         {bgfx::RendererType::Count, nullptr, 0}}},
    {"fs_color",
     {
#if defined(_WIN32)
         {bgfx::RendererType::Direct3D11, fs_color_dxbc, sizeof(fs_color_dxbc)},
#endif
         {bgfx::RendererType::OpenGLES, fs_color_essl, sizeof(fs_color_essl)},
         {bgfx::RendererType::OpenGL, fs_color_glsl, sizeof(fs_color_glsl)},
         {bgfx::RendererType::Vulkan, fs_color_spv, sizeof(fs_color_spv)},
         {bgfx::RendererType::Count, nullptr, 0}}},
    BGFX_EMBEDDED_SHADER_END()};

static const bgfx::EmbeddedShader text_shaders[] = {
    {"vs_text",
     {
#if defined(_WIN32)
         {bgfx::RendererType::Direct3D11, vs_text_dxbc, sizeof(vs_text_dxbc)},
#endif
         {bgfx::RendererType::OpenGLES, vs_text_essl, sizeof(vs_text_essl)},
         {bgfx::RendererType::OpenGL, vs_text_glsl, sizeof(vs_text_glsl)},
         {bgfx::RendererType::Vulkan, vs_text_spv, sizeof(vs_text_spv)},
         {bgfx::RendererType::Count, nullptr, 0}}},
    {"fs_text",
     {
#if defined(_WIN32)
         {bgfx::RendererType::Direct3D11, fs_text_dxbc, sizeof(fs_text_dxbc)},
#endif
         {bgfx::RendererType::OpenGLES, fs_text_essl, sizeof(fs_text_essl)},
         {bgfx::RendererType::OpenGL, fs_text_glsl, sizeof(fs_text_glsl)},
         {bgfx::RendererType::Vulkan, fs_text_spv, sizeof(fs_text_spv)},
         {bgfx::RendererType::Count, nullptr, 0}}},
    BGFX_EMBEDDED_SHADER_END()};

std::uint32_t pack_color(color value)
{
    return std::uint32_t(value.r) | (std::uint32_t(value.g) << 8) | (std::uint32_t(value.b) << 16) | (std::uint32_t(value.a) << 24);
}

} // namespace centui::details

export namespace centui
{

class renderer
{
public:
    explicit renderer(font& face) : _font(face)
    {
        _vertex_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();

        _text_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();

        const auto type = bgfx::getRendererType();

        const auto vs = bgfx::createEmbeddedShader(details::color_shaders, type, "vs_color");
        const auto fs = bgfx::createEmbeddedShader(details::color_shaders, type, "fs_color");

        if(!bgfx::isValid(vs) || !bgfx::isValid(fs))
        {
            if(bgfx::isValid(vs))
            {
                bgfx::destroy(vs);
            }

            if(bgfx::isValid(fs))
            {
                bgfx::destroy(fs);
            }

            throw std::runtime_error("Failed to create CentUI color shaders");
        }

        _program = bgfx::createProgram(vs, fs, true);

        if(!bgfx::isValid(_program))
        {
            throw std::runtime_error("Failed to create CentUI color program");
        }

        const auto text_vs = bgfx::createEmbeddedShader(details::text_shaders, type, "vs_text");
        const auto text_fs = bgfx::createEmbeddedShader(details::text_shaders, type, "fs_text");

        if(!bgfx::isValid(text_vs) || !bgfx::isValid(text_fs))
        {
            if(bgfx::isValid(text_vs))
            {
                bgfx::destroy(text_vs);
            }

            if(bgfx::isValid(text_fs))
            {
                bgfx::destroy(text_fs);
            }

            bgfx::destroy(_program);
            _program = BGFX_INVALID_HANDLE;

            throw std::runtime_error("Failed to create CentUI text shaders");
        }

        _text_program = bgfx::createProgram(text_vs, text_fs, true);

        if(!bgfx::isValid(_text_program))
        {
            bgfx::destroy(_program);
            _program = BGFX_INVALID_HANDLE;

            throw std::runtime_error("Failed to create CentUI text program");
        }

        _sampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

        if(!bgfx::isValid(_sampler))
        {
            bgfx::destroy(_text_program);
            bgfx::destroy(_program);

            _text_program = BGFX_INVALID_HANDLE;
            _program = BGFX_INVALID_HANDLE;

            throw std::runtime_error("Failed to create CentUI text sampler");
        }
    }

    ~renderer()
    {
        for(const auto& [key, glyph]: _glyphs)
        {
            if(bgfx::isValid(glyph.texture))
            {
                bgfx::destroy(glyph.texture);
            }
        }

        if(bgfx::isValid(_sampler))
        {
            bgfx::destroy(_sampler);
        }

        if(bgfx::isValid(_text_program))
        {
            bgfx::destroy(_text_program);
        }

        if(bgfx::isValid(_program))
        {
            bgfx::destroy(_program);
        }
    }

    renderer(const renderer&) = delete;
    renderer& operator=(const renderer&) = delete;

    void begin(std::uint16_t width, std::uint16_t height)
    {
        bgfx::setViewMode(0, bgfx::ViewMode::Sequential);

        float projection[16];
        bx::mtxOrtho(projection, 0.0f, float(width), float(height), 0.0f, 0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

        bgfx::setViewTransform(0, nullptr, projection);
    }

    void rectangle(const rect& bounds, color value)
    {
        if(value.a == 0 || bounds.width <= 0 || bounds.height <= 0)
        {
            return;
        }

        if(bgfx::getAvailTransientVertexBuffer(4, _vertex_layout) < 4 || bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        bgfx::TransientVertexBuffer vertices;
        bgfx::TransientIndexBuffer  indices;

        bgfx::allocTransientVertexBuffer(&vertices, 4, _vertex_layout);
        bgfx::allocTransientIndexBuffer(&indices, 6);

        const auto packed = details::pack_color(value);
        auto*      v = reinterpret_cast<details::color_vertex*>(vertices.data);

        v[0] = {bounds.x, bounds.y, 0.0f, packed};
        v[1] = {bounds.x + bounds.width, bounds.y, 0.0f, packed};
        v[2] = {bounds.x + bounds.width, bounds.y + bounds.height, 0.0f, packed};
        v[3] = {bounds.x, bounds.y + bounds.height, 0.0f, packed};

        auto* i = reinterpret_cast<std::uint16_t*>(indices.data);

        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
        i[3] = 0;
        i[4] = 2;
        i[5] = 3;

        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setIndexBuffer(&indices);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
        bgfx::submit(0, _program);
    }

    void text(const text_command& command)
    {
        if(command.color.a == 0 || command.value.empty())
        {
            return;
        }

        const auto shaped = _font.shape(command.value, command.font_size);
        const auto baseline = command.bounds.y + _font.ascender(command.font_size);

        for(const auto& item: shaped)
        {
            const auto& cached = get_glyph(item.id, command.font_size);

            rect bounds{
                .x = command.bounds.x + item.x + cached.left,
                .y = baseline - item.y - cached.top,
                .width = static_cast<float>(cached.width),
                .height = static_cast<float>(cached.height)};

            glyph(bounds, command.color, cached.texture);
        }
    }

    void draw(const node& root)
    {
        for(const auto& command: commands(root))
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
        bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
        int                 width = 0;
        int                 height = 0;
        int                 left = 0;
        int                 top = 0;
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

            entry.texture = bgfx::createTexture2D(
                static_cast<std::uint16_t>(entry.width),
                static_cast<std::uint16_t>(entry.height),
                false,
                1,
                bgfx::TextureFormat::BGRA8,
                BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
                bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size() * sizeof(std::uint32_t))));

            if(!bgfx::isValid(entry.texture))
            {
                throw std::runtime_error("Failed to create glyph texture");
            }
        }

        return _glyphs.emplace(key, entry).first->second;
    }

    void glyph(const rect& bounds, color value, bgfx::TextureHandle texture)
    {
        if(!bgfx::isValid(texture) || bounds.width <= 0 || bounds.height <= 0 || value.a == 0)
        {
            return;
        }

        if(bgfx::getAvailTransientVertexBuffer(4, _text_layout) < 4 || bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        bgfx::TransientVertexBuffer vertices;
        bgfx::TransientIndexBuffer  indices;

        bgfx::allocTransientVertexBuffer(&vertices, 4, _text_layout);
        bgfx::allocTransientIndexBuffer(&indices, 6);

        const auto packed = details::pack_color(value);
        auto*      v = reinterpret_cast<details::text_vertex*>(vertices.data);

        v[0] = {bounds.x, bounds.y, 0.0f, 0.0f, 0.0f, packed};
        v[1] = {bounds.x + bounds.width, bounds.y, 0.0f, 1.0f, 0.0f, packed};
        v[2] = {bounds.x + bounds.width, bounds.y + bounds.height, 0.0f, 1.0f, 1.0f, packed};
        v[3] = {bounds.x, bounds.y + bounds.height, 0.0f, 0.0f, 1.0f, packed};

        auto* i = reinterpret_cast<std::uint16_t*>(indices.data);

        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
        i[3] = 0;
        i[4] = 2;
        i[5] = 3;

        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setIndexBuffer(&indices);
        bgfx::setTexture(0, _sampler, texture);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
        bgfx::submit(0, _text_program);
    }

    font& _font;

    bgfx::VertexLayout _vertex_layout;
    bgfx::VertexLayout _text_layout;

    bgfx::ProgramHandle _program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle _text_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle _sampler = BGFX_INVALID_HANDLE;

    std::unordered_map<std::uint64_t, cached_glyph> _glyphs;
};

} // namespace centui
