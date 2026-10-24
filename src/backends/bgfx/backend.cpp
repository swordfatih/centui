#include <centui/backends/bgfx.hpp>

// Generated platform support macros must precede the embedded shader header.
// clang-format off
#include <centui_shaders.h>
#include <bgfx/embedded_shader.h>
// clang-format on
#include <bx/math.h>

namespace
{

static const ::bgfx::EmbeddedShader color_shaders[] = {
    BGFX_EMBEDDED_SHADER(vs_color),
    BGFX_EMBEDDED_SHADER(fs_color),
    BGFX_EMBEDDED_SHADER_END(),
};

static const ::bgfx::EmbeddedShader text_shaders[] = {
    BGFX_EMBEDDED_SHADER(vs_text),
    BGFX_EMBEDDED_SHADER(fs_text),
    BGFX_EMBEDDED_SHADER_END(),
};

} // namespace

namespace centui::details
{

struct color_vertex
{
    float x;
    float y;
    float z;
    std::uint32_t rgba;
};

struct text_vertex
{
    float x;
    float y;
    float z;
    float u;
    float v;
    std::uint32_t rgba;
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

std::uint32_t pack_color(color value)
{
    return std::uint32_t(value.r) | (std::uint32_t(value.g) << 8) | (std::uint32_t(value.b) << 16) |
           (std::uint32_t(value.a) << 24);
}

} // namespace centui::details

namespace centui::bgfx
{

std::unique_ptr<backend::context> backend::create_context(native_window_handles handles, centui::viewport size)
{
    if(!handles.window || size.width <= 0 || size.height <= 0)
    {
        throw std::invalid_argument("bgfx requires a native window with positive dimensions");
    }

    ::bgfx::Init init;
    init.swapChain.nwh = handles.window;
    init.swapChain.ndt = handles.display;
    init.platformData.type = handles.system == native_window_system::wayland ? ::bgfx::NativeWindowHandleType::Wayland
                                                                             : ::bgfx::NativeWindowHandleType::Default;
    init.swapChain.width = static_cast<std::uint32_t>(size.width);
    init.swapChain.height = static_cast<std::uint32_t>(size.height);
    init.reset = BGFX_RESET_VSYNC;

    return std::make_unique<context>(init);
}

backend::backend(const centui::viewport& target, std::uint16_t view_id)
    : backend(
          [&target]
          {
              return target;
          },
          {},
          view_id
      )
{
}

backend::context::context(const ::bgfx::Init& init)
{
    if(!::bgfx::init(init))
    {
        throw std::runtime_error("Failed to initialize bgfx");
    }
}

backend::context::~context()
{
    ::bgfx::shutdown();
}

backend::backend(std::function<centui::viewport()> surface, std::unique_ptr<context> owner, std::uint16_t view_id)
    : m_context(std::move(owner)),
      m_surface(std::move(surface)),
      m_view_id(view_id)
{
    m_backbuffer = viewport();
    m_vertex_layout.begin()
        .add(::bgfx::Attrib::Position, 3, ::bgfx::AttribType::Float)
        .add(::bgfx::Attrib::Color0, 4, ::bgfx::AttribType::Uint8, true)
        .end();

    m_text_layout.begin()
        .add(::bgfx::Attrib::Position, 3, ::bgfx::AttribType::Float)
        .add(::bgfx::Attrib::TexCoord0, 2, ::bgfx::AttribType::Float)
        .add(::bgfx::Attrib::Color0, 4, ::bgfx::AttribType::Uint8, true)
        .end();

    m_program = details::owned_handle{details::create_program(color_shaders, "vs_color", "fs_color")};
    m_text_program = details::owned_handle{details::create_program(text_shaders, "vs_text", "fs_text")};
    m_sampler = details::owned_handle{::bgfx::createUniform("s_texColor", ::bgfx::UniformType::Sampler)};

    if(!::bgfx::isValid(m_sampler.get()))
    {
        throw std::runtime_error("Failed to create CentUI text sampler");
    }
}

centui::viewport backend::viewport() const
{
    return m_surface();
}

bool backend::begin_frame(color clear)
{
    if(!m_context)
    {
        throw std::logic_error("Frame lifecycle requires a backend constructed from a platform");
    }

    const auto size = viewport();

    if(size.width <= 0 || size.height <= 0)
    {
        return false;
    }

    if(size.width > std::numeric_limits<std::uint16_t>::max() ||
       size.height > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::invalid_argument("Window exceeds bgfx view dimensions");
    }

    if(size.width != m_backbuffer.width || size.height != m_backbuffer.height)
    {
        ::bgfx::SwapChain chain;
        chain.width = static_cast<std::uint32_t>(size.width);
        chain.height = static_cast<std::uint32_t>(size.height);
        ::bgfx::reset(BGFX_RESET_VSYNC, &chain);
        m_backbuffer = size;
    }

    const auto rgba =
        (std::uint32_t{clear.r} << 24) | (std::uint32_t{clear.g} << 16) | (std::uint32_t{clear.b} << 8) | clear.a;
    ::bgfx::setViewRect(
        m_view_id,
        0,
        0,
        static_cast<std::uint16_t>(size.width),
        static_cast<std::uint16_t>(size.height)
    );
    ::bgfx::setViewClear(m_view_id, BGFX_CLEAR_COLOR, rgba);
    ::bgfx::touch(m_view_id);

    return true;
}

void backend::end_frame()
{
    if(!m_context)
    {
        throw std::logic_error("Frame lifecycle requires a backend constructed from a platform");
    }

    ::bgfx::frame();
}

void backend::begin(std::uint16_t width, std::uint16_t height, float scale)
{
    ::bgfx::setViewRect(m_view_id, 0, 0, width, height);
    ::bgfx::setViewMode(m_view_id, ::bgfx::ViewMode::Sequential);

    std::array<float, 16> projection{};
    bx::mtxOrtho(
        projection.data(),
        0.0f,
        width / scale,
        height / scale,
        0.0f,
        0.0f,
        100.0f,
        0.0f,
        ::bgfx::getCaps()->homogeneousDepth
    );

    ::bgfx::setViewTransform(m_view_id, nullptr, projection.data());
}

void backend::rectangle(const rect& bounds, color value)
{
    if(value.a == 0 || bounds.width <= 0 || bounds.height <= 0)
    {
        return;
    }

    if(::bgfx::getAvailTransientVertexBuffer(4, m_vertex_layout) < 4 || ::bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    ::bgfx::TransientVertexBuffer vertices;
    ::bgfx::TransientIndexBuffer indices;

    ::bgfx::allocTransientVertexBuffer(&vertices, 4, m_vertex_layout);
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
    ::bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_BLEND_FUNC_SEPARATE(
            BGFX_STATE_BLEND_SRC_ALPHA,
            BGFX_STATE_BLEND_INV_SRC_ALPHA,
            BGFX_STATE_BLEND_ONE,
            BGFX_STATE_BLEND_INV_SRC_ALPHA
        )
    );
    ::bgfx::submit(m_view_id, m_program.get());
}

void backend::set_cache_budget(std::size_t bytes)
{
    m_cache_budget = bytes;
}

centui::details::texture_cache_statistics backend::cache_statistics() const
{
    return m_glyphs.statistics();
}

void backend::clear_cache()
{
    m_glyphs.clear();
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

    if(size.width > std::numeric_limits<std::uint16_t>::max() ||
       size.height > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::invalid_argument("UI viewport exceeds bgfx view dimensions");
    }

    m_glyphs.begin_frame(list, m_cache_limit, m_cache_budget);
    begin(static_cast<std::uint16_t>(size.width), static_cast<std::uint16_t>(size.height), size.scale);
    for(const auto& item: list.commands)
    {
        if(item.image)
        {
            glyph(item.bounds, item.color, item.uv, get_glyph(item.image).get(), item.sampling);
        }
        else
        {
            rectangle(item.bounds, item.color);
        }
    }
}

const backend::texture_handle& backend::get_glyph(const std::shared_ptr<const bitmap>& bitmap)
{
    const std::weak_ptr<const centui::bitmap> key{bitmap};

    if(const auto found = m_glyphs.find(key); found != m_glyphs.end())
    {
        return found->second;
    }

    const auto limit = ::bgfx::getCaps()->limits.maxTextureSize;

    if(bitmap->width <= 0 || bitmap->height <= 0 || std::cmp_greater(bitmap->width, limit) ||
       std::cmp_greater(bitmap->height, limit) || bitmap->width > std::numeric_limits<std::uint16_t>::max() ||
       bitmap->height > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::invalid_argument("Glyph image exceeds bgfx texture dimensions");
    }

    if(bitmap->pixels.size() > std::numeric_limits<std::uint32_t>::max() / sizeof(std::uint32_t))
    {
        throw std::length_error("Glyph image exceeds bgfx upload size");
    }

    const auto pixels = rgba_pixels(*bitmap);
    details::owned_handle texture{::bgfx::createTexture2D(
        static_cast<std::uint16_t>(bitmap->width),
        static_cast<std::uint16_t>(bitmap->height),
        false,
        1,
        ::bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        ::bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size()))
    )};

    if(!::bgfx::isValid(texture.get()))
    {
        throw std::runtime_error("Failed to create glyph texture");
    }

    return m_glyphs.emplace(key, std::move(texture)).first->second;
}

void backend::glyph(const rect& bounds, color value, rect uv, ::bgfx::TextureHandle texture, image_sampling sampling)
{
    if(!::bgfx::isValid(texture) || bounds.width <= 0 || bounds.height <= 0 || value.a == 0)
    {
        return;
    }

    if(::bgfx::getAvailTransientVertexBuffer(4, m_text_layout) < 4 || ::bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    ::bgfx::TransientVertexBuffer vertices;
    ::bgfx::TransientIndexBuffer indices;

    ::bgfx::allocTransientVertexBuffer(&vertices, 4, m_text_layout);
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
    const auto flags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
                       (sampling == image_sampling::nearest ? BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT : 0);
    ::bgfx::setTexture(0, m_sampler.get(), texture, flags);
    ::bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_BLEND_FUNC_SEPARATE(
            BGFX_STATE_BLEND_SRC_ALPHA,
            BGFX_STATE_BLEND_INV_SRC_ALPHA,
            BGFX_STATE_BLEND_ONE,
            BGFX_STATE_BLEND_INV_SRC_ALPHA
        )
    );
    ::bgfx::submit(m_view_id, m_text_program.get());
}

} // namespace centui::bgfx
