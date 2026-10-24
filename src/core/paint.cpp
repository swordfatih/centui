#include "paint_service.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <stdexcept>
#include <utility>

#include <lunasvg.h>
#include <plutovg.h>

namespace centui
{
namespace
{

struct path_deleter
{
    void operator()(plutovg_path_t* value) const noexcept
    {
        plutovg_path_destroy(value);
    }
};

struct canvas_deleter
{
    void operator()(plutovg_canvas_t* value) const noexcept
    {
        plutovg_canvas_destroy(value);
    }
};

using native_path = std::unique_ptr<plutovg_path_t, path_deleter>;
using native_canvas = std::unique_ptr<plutovg_canvas_t, canvas_deleter>;

void finite(vec2 point)
{
    if(!std::isfinite(point.x) || !std::isfinite(point.y))
    {
        throw std::invalid_argument("Path coordinates must be finite");
    }
}

void nonnegative(float value)
{
    if(!std::isfinite(value) || value < 0)
    {
        throw std::invalid_argument("Drawing dimensions must be finite and nonnegative");
    }
}

void validate(rect bounds)
{
    finite({bounds.x, bounds.y});
    nonnegative(bounds.width);
    nonnegative(bounds.height);
    finite({bounds.x + bounds.width, bounds.y + bounds.height});
}

void apply_brush(plutovg_canvas_t* canvas, const brush& paint)
{
    std::visit(
        [&](const auto& value)
        {
            using type = std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<type, color>)
            {
                plutovg_canvas_set_rgba(canvas, value.r / 255.0f, value.g / 255.0f, value.b / 255.0f, value.a / 255.0f);
            }
            else
            {
                if(value.stops.empty() ||
                   value.stops.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                {
                    throw std::invalid_argument("Gradients require a representable, nonempty stop list");
                }
                std::vector<plutovg_gradient_stop_t> stops;
                stops.reserve(value.stops.size());
                float previous = 0;
                for(const auto& stop: value.stops)
                {
                    if(!std::isfinite(stop.offset) || stop.offset < previous || stop.offset > 1)
                    {
                        throw std::invalid_argument("Gradient stops must be ordered between zero and one");
                    }
                    previous = stop.offset;
                    const auto c = stop.color;
                    stops.push_back({stop.offset, {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}});
                }
                if constexpr(std::is_same_v<type, linear_gradient>)
                {
                    finite(value.start);
                    finite(value.end);
                    plutovg_canvas_set_linear_gradient(
                        canvas,
                        value.start.x,
                        value.start.y,
                        value.end.x,
                        value.end.y,
                        PLUTOVG_SPREAD_METHOD_PAD,
                        stops.data(),
                        static_cast<int>(stops.size()),
                        nullptr
                    );
                }
                else
                {
                    finite(value.center);
                    nonnegative(value.radius);
                    if(value.radius == 0)
                    {
                        throw std::invalid_argument("Radial gradients require a positive radius");
                    }
                    plutovg_canvas_set_radial_gradient(
                        canvas,
                        value.center.x,
                        value.center.y,
                        value.radius,
                        value.center.x,
                        value.center.y,
                        0,
                        PLUTOVG_SPREAD_METHOD_PAD,
                        stops.data(),
                        static_cast<int>(stops.size()),
                        nullptr
                    );
                }
            }
        },
        paint
    );
}

details::painted_image rasterize(
    plutovg_path_t* shape,
    rect bounds,
    const brush& paint,
    float stroke,
    float scale,
    bool even_odd
)
{
    // Bounds are a physical tile, including its filtering border.
    const auto left = bounds.x;
    const auto top = bounds.y;
    const auto width = bounds.width;
    const auto height = bounds.height;
    if(width < 1 || height < 1 || width > 512 || height > 512)
    {
        throw std::length_error("Vector tile exceeds raster storage limits");
    }

    lunasvg::Bitmap surface{static_cast<int>(width), static_cast<int>(height)};
    if(surface.isNull())
    {
        throw std::runtime_error("Failed to allocate vector surface");
    }
    surface.clear(0);
    native_canvas canvas{plutovg_canvas_create(surface.surface())};
    if(!canvas)
    {
        throw std::runtime_error("Failed to create vector canvas");
    }
    plutovg_matrix_t matrix;
    plutovg_matrix_init(&matrix, scale, 0, 0, scale, static_cast<float>(-left), static_cast<float>(-top));
    plutovg_canvas_set_matrix(canvas.get(), &matrix);
    apply_brush(canvas.get(), paint);
    if(stroke > 0)
    {
        plutovg_canvas_set_line_width(canvas.get(), stroke);
        plutovg_canvas_set_line_cap(canvas.get(), PLUTOVG_LINE_CAP_ROUND);
        plutovg_canvas_set_line_join(canvas.get(), PLUTOVG_LINE_JOIN_ROUND);
        plutovg_canvas_stroke_path(canvas.get(), shape);
    }
    else
    {
        plutovg_canvas_set_fill_rule(canvas.get(), even_odd ? PLUTOVG_FILL_RULE_EVEN_ODD : PLUTOVG_FILL_RULE_NON_ZERO);
        plutovg_canvas_fill_path(canvas.get(), shape);
    }
    surface.convertToRGBA();
    bitmap result{surface.width(), surface.height(), {}, pixel_format::rgba};
    const auto row_bytes = static_cast<std::size_t>(result.width) * 4;
    result.pixels.resize(row_bytes * result.height);
    for(int y = 0; y < result.height; ++y)
    {
        std::copy_n(
            surface.data() + static_cast<std::size_t>(y) * surface.stride(),
            row_bytes,
            result.pixels.data() + static_cast<std::size_t>(y) * row_bytes
        );
    }
    if(const auto* solid = std::get_if<color>(&paint))
    {
        // Straight-alpha filtering needs the mask color even in transparent padding.
        for(std::size_t i = 0; i < result.pixels.size(); i += 4)
        {
            result.pixels[i] = solid->r;
            result.pixels[i + 1] = solid->g;
            result.pixels[i + 2] = solid->b;
        }
    }
    return {
        {static_cast<float>(left / scale),
         static_cast<float>(top / scale),
         result.width / scale,
         result.height / scale},
        {},
        std::make_shared<const bitmap>(std::move(result))
    };
}

// Inner radii account for each physical border independently.
void inner_contour(plutovg_path_t* shape, size size, float radius, computed_edges edges)
{
    const auto x = edges.left;
    const auto y = edges.top;
    const auto right = size.width - edges.right;
    const auto bottom = size.height - edges.bottom;
    if(right <= x || bottom <= y)
    {
        return;
    }
    const auto rx0 = std::min((right - x) / 2, std::max(0.0f, radius - edges.left));
    const auto rx1 = std::min((right - x) / 2, std::max(0.0f, radius - edges.right));
    const auto ry0 = std::min((bottom - y) / 2, std::max(0.0f, radius - edges.top));
    const auto ry1 = std::min((bottom - y) / 2, std::max(0.0f, radius - edges.bottom));
    constexpr float k = 0.55228475f;
    plutovg_path_move_to(shape, x + rx0, y);
    plutovg_path_line_to(shape, right - rx1, y);
    plutovg_path_cubic_to(shape, right - rx1 + k * rx1, y, right, y + ry0 - k * ry0, right, y + ry0);
    plutovg_path_line_to(shape, right, bottom - ry1);
    plutovg_path_cubic_to(shape, right, bottom - ry1 + k * ry1, right - rx1 + k * rx1, bottom, right - rx1, bottom);
    plutovg_path_line_to(shape, x + rx0, bottom);
    plutovg_path_cubic_to(shape, x + rx0 - k * rx0, bottom, x, bottom - ry1 + k * ry1, x, bottom - ry1);
    plutovg_path_line_to(shape, x, y + ry0);
    plutovg_path_cubic_to(shape, x, y + ry0 - k * ry0, x + rx0 - k * rx0, y, x + rx0, y);
    plutovg_path_close(shape);
}

} // namespace

struct path::data
{
    // Cache keys retain only this token, never the native geometry.
    std::shared_ptr<const int> revision = std::make_shared<const int>(0);
    native_path native{plutovg_path_create()};

    data()
    {
        if(!native)
        {
            throw std::bad_alloc();
        }
    }

    data(const data& other)
        : native(plutovg_path_clone(other.native.get()))
    {
        if(!native)
        {
            throw std::bad_alloc();
        }
    }
};

path::path()
    : m_data(std::make_shared<data>())
{
}

path::data& path::writable()
{
    if(!m_data)
    {
        m_data = std::make_shared<data>();
    }
    else if(!m_data.unique())
    {
        m_data = std::make_shared<data>(*m_data);
    }
    else if(!m_data->revision.unique())
    {
        // A cached raster observes the old revision. Editing an otherwise
        // unshared path needs a new identity, but no geometry copy.
        m_data->revision = std::make_shared<const int>(0);
    }
    return *m_data;
}

path& path::move_to(vec2 point)
{
    finite(point);
    plutovg_path_move_to(writable().native.get(), point.x, point.y);
    return *this;
}

path& path::line_to(vec2 point)
{
    finite(point);
    plutovg_path_line_to(writable().native.get(), point.x, point.y);
    return *this;
}

path& path::curve_to(vec2 control1, vec2 control2, vec2 end)
{
    finite(control1);
    finite(control2);
    finite(end);
    plutovg_path_cubic_to(writable().native.get(), control1.x, control1.y, control2.x, control2.y, end.x, end.y);
    return *this;
}

path& path::close()
{
    plutovg_path_close(writable().native.get());
    return *this;
}

path& path::rectangle(rect bounds)
{
    validate(bounds);
    plutovg_path_add_rect(writable().native.get(), bounds.x, bounds.y, bounds.width, bounds.height);
    return *this;
}

path& path::rounded_rectangle(rect bounds, float radius)
{
    validate(bounds);
    nonnegative(radius);
    radius = std::min({radius, bounds.width / 2, bounds.height / 2});
    plutovg_path_add_round_rect(
        writable().native.get(),
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        radius,
        radius
    );
    return *this;
}

path& path::circle(vec2 center, float radius)
{
    finite(center);
    nonnegative(radius);
    validate({center.x - radius, center.y - radius, radius * 2, radius * 2});
    plutovg_path_add_circle(writable().native.get(), center.x, center.y, radius);
    return *this;
}

namespace details
{

struct paint_service::data
{
    struct key
    {
        std::shared_ptr<const int> source;
        float width = 0;
        float height = 0;
        float radius = 0;
        computed_edges border{};
        rect tile{};
        float scale = 1;
        float stroke = 0;
        brush paint = color{255, 255, 255, 255};
        bool operator==(const key&) const = default;
    };

    struct entry
    {
        key request;
        painted_image result;
        std::size_t bytes;
    };

    std::list<entry> cache;
    std::size_t bytes = 0;
    std::size_t budget = 16 * 1024 * 1024;

    template <typename Prepare>
    std::vector<painted_image> tiles(key request, rect bounds, rect visible, Prepare&& prepare)
    {
        validate(bounds);
        validate(visible);
        const double scale = request.scale;
        if(!std::isfinite(scale) || scale <= 0)
        {
            throw std::invalid_argument("Drawing scale must be finite and positive");
        }
        if(visible.width == 0 || visible.height == 0)
        {
            return {};
        }

        const double pad = request.stroke / 2.0 + 1.0 / scale;
        const double left = std::floor((bounds.x - pad) * scale);
        const double top = std::floor((bounds.y - pad) * scale);
        const double right = std::ceil((bounds.x + static_cast<double>(bounds.width) + pad) * scale);
        const double bottom = std::ceil((bounds.y + static_cast<double>(bounds.height) + pad) * scale);
        // Keep transformed coordinates representable by PlutoVG's fixed-point rasterizer.
        constexpr double coordinate_limit = 4194304;
        for(const double edge: {left, top, right, bottom})
        {
            if(!std::isfinite(edge) || std::abs(edge) > coordinate_limit)
            {
                throw std::length_error("Vector coordinates exceed rasterizer limits");
            }
        }
        const double x0 = std::max(left, std::floor(static_cast<double>(visible.x) * scale));
        const double y0 = std::max(top, std::floor(static_cast<double>(visible.y) * scale));
        const double x1 = std::min(right, std::ceil((visible.x + static_cast<double>(visible.width)) * scale));
        const double y1 = std::min(bottom, std::ceil((visible.y + static_cast<double>(visible.height)) * scale));
        if(x1 <= x0 || y1 <= y0)
        {
            return {};
        }

        // 510 interior pixels plus a one-pixel filtering border on each side.
        // The grid belongs to the shape, never to the scrolling clip.
        constexpr int step = 510;
        const int first_x = static_cast<int>(std::floor((x0 - left) / step));
        const int first_y = static_cast<int>(std::floor((y0 - top) / step));
        const int end_x = static_cast<int>(std::ceil((x1 - left) / step));
        const int end_y = static_cast<int>(std::ceil((y1 - top) / step));
        std::vector<painted_image> result;
        for(int y = first_y; y < end_y; ++y)
        {
            for(int x = first_x; x < end_x; ++x)
            {
                const double l = left + x * step;
                const double t = top + y * step;
                const double r = std::min(right, l + step);
                const double b = std::min(bottom, t + step);
                const double raster_left = std::max(left, l - 1);
                const double raster_top = std::max(top, t - 1);
                request.tile = {
                    static_cast<float>(raster_left),
                    static_cast<float>(raster_top),
                    static_cast<float>(std::min(right, r + 1) - raster_left),
                    static_cast<float>(std::min(bottom, b + 1) - raster_top)
                };
                auto tile =
                    get(request,
                        [&]
                        {
                            return prepare(request.tile);
                        });
                tile.clip = {
                    static_cast<float>(l / scale),
                    static_cast<float>(t / scale),
                    static_cast<float>((r - l) / scale),
                    static_cast<float>((b - t) / scale)
                };
                result.push_back(std::move(tile));
            }
        }
        return result;
    }

    template <typename Prepare>
    painted_image get(key request, Prepare&& prepare)
    {
        const auto found = std::find_if(
            cache.begin(),
            cache.end(),
            [&](const auto& item)
            {
                return item.request == request;
            }
        );
        if(found != cache.end())
        {
            cache.splice(cache.begin(), cache, found);
            return cache.front().result;
        }
        auto result = prepare();
        constexpr std::size_t overhead = sizeof(entry) + sizeof(bitmap) + 128;
        std::size_t stops = std::visit(
            [](const auto& value) -> std::size_t
            {
                if constexpr(std::is_same_v<std::decay_t<decltype(value)>, color>)
                {
                    return 0;
                }
                else
                {
                    return value.stops.size() * sizeof(gradient_stop);
                }
            },
            request.paint
        );
        if(result.image && budget >= overhead && stops <= budget - overhead &&
           result.image->pixels.size() <= budget - overhead - stops)
        {
            const auto cost = result.image->pixels.size() + overhead + stops;
            while(bytes > budget - cost)
            {
                bytes -= cache.back().bytes;
                cache.pop_back();
            }
            cache.push_front({std::move(request), result, cost});
            bytes += cost;
        }
        return result;
    }
};

paint_service::paint_service()
    : m_data(std::make_unique<data>())
{
}

paint_service::~paint_service() = default;

std::vector<painted_image> paint_service::prepare(
    const path& shape,
    const brush& paint,
    float stroke,
    float scale,
    rect visible
)
{
    nonnegative(stroke);
    if(!shape.m_data)
    {
        return {};
    }
    data::key request;
    request.source = shape.m_data->revision;
    request.paint = paint;
    request.stroke = stroke;
    request.scale = scale;
    plutovg_rect_t bounds;
    // Control-point bounds are conservative and also bound transformed coordinates.
    plutovg_path_extents(shape.m_data->native.get(), &bounds, false);
    return m_data->tiles(
        request,
        {bounds.x, bounds.y, bounds.w, bounds.h},
        visible,
        [&](rect tile)
        {
            return rasterize(shape.m_data->native.get(), tile, paint, stroke, scale, false);
        }
    );
}

std::vector<painted_image> paint_service::box(
    size extent,
    float radius,
    computed_edges border,
    float scale,
    rect visible
)
{
    nonnegative(extent.width);
    nonnegative(extent.height);
    nonnegative(radius);
    if(extent.width == 0 || extent.height == 0)
    {
        return {};
    }
    radius = std::min({radius, extent.width / 2, extent.height / 2});
    data::key request;
    request.width = extent.width;
    request.height = extent.height;
    request.radius = radius;
    request.border = border;
    request.scale = scale;
    native_path shape;
    const bool ring = border.top > 0 || border.right > 0 || border.bottom > 0 || border.left > 0;
    return m_data->tiles(
        request,
        {0, 0, extent.width, extent.height},
        visible,
        [&](rect tile)
        {
            if(!shape)
            {
                shape.reset(plutovg_path_create());
                if(!shape)
                {
                    throw std::bad_alloc();
                }
                plutovg_path_add_round_rect(shape.get(), 0, 0, extent.width, extent.height, radius, radius);
                if(ring)
                {
                    inner_contour(shape.get(), extent, radius, border);
                }
            }
            return rasterize(shape.get(), tile, request.paint, 0, scale, ring);
        }
    );
}

void paint_service::set_cache_budget(std::size_t bytes)
{
    m_data->budget = bytes;
    while(m_data->bytes > bytes)
    {
        m_data->bytes -= m_data->cache.back().bytes;
        m_data->cache.pop_back();
    }
}

} // namespace details
} // namespace centui
