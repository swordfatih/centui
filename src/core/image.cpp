#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <centui/image.hpp>

#include <lunasvg.h>
#include <plutovg.h>

namespace centui
{
namespace
{

std::size_t pixel_bytes(int width, int height)
{
    if(width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() / 4 ||
       static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() / 4 / height)
    {
        throw std::length_error("Image dimensions exceed raster storage limits");
    }

    return static_cast<std::size_t>(width) * height * 4;
}

int physical_dimension(float logical, float scale)
{
    if(!std::isfinite(logical) || logical <= 0 || !std::isfinite(scale) || scale <= 0)
    {
        throw std::invalid_argument("Image size and display scale must be finite and positive");
    }
    const auto pixels = std::ceil(static_cast<double>(logical) * scale);
    if(pixels > std::numeric_limits<int>::max())
    {
        throw std::length_error("Image physical dimensions are not representable");
    }
    return static_cast<int>(pixels);
}

std::shared_ptr<const bitmap> convert(lunasvg::Bitmap image)
{
    if(image.isNull())
    {
        throw std::runtime_error("Failed to decode or rasterize image");
    }

    bitmap result{image.width(), image.height(), {}, pixel_format::rgba};
    result.pixels.resize(pixel_bytes(result.width, result.height));
    image.convertToRGBA();
    const auto stride = static_cast<std::size_t>(result.width) * 4;

    for(int row = 0; row < result.height; ++row)
    {
        std::copy_n(
            image.data() + static_cast<std::size_t>(row) * image.stride(),
            stride,
            result.pixels.data() + static_cast<std::size_t>(row) * stride
        );
    }

    return std::make_shared<const bitmap>(std::move(result));
}

std::string_view trim(std::string_view text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    return begin == std::string_view::npos ? std::string_view{}
                                           : text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}

std::optional<float> dimension(std::string_view text)
{
    text = trim(text);
    if(text.empty())
    {
        return {};
    }

    float value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if(parsed.ec != std::errc{} || !std::isfinite(value) || value <= 0)
    {
        return {};
    }

    const auto suffix = trim(text.substr(static_cast<std::size_t>(parsed.ptr - text.data())));
    float factor = 1;
    if(suffix == "in")
    {
        factor = 96;
    }
    else if(suffix == "cm")
    {
        factor = 96 / 2.54f;
    }
    else if(suffix == "mm")
    {
        factor = 96 / 25.4f;
    }
    else if(suffix == "pt")
    {
        factor = 96 / 72.0f;
    }
    else if(suffix == "pc")
    {
        factor = 16;
    }
    else if(!suffix.empty() && suffix != "px")
    {
        return {};
    }

    value *= factor;
    return std::isfinite(value) ? std::optional<float>{value} : std::nullopt;
}

std::optional<float> viewbox_ratio(std::string value)
{
    std::replace(value.begin(), value.end(), ',', ' ');
    std::istringstream input{value};
    input.imbue(std::locale::classic());
    float x = 0, y = 0, width = 0, height = 0;
    if(input >> x >> y >> width >> height && std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
       std::isfinite(height) && width > 0 && height > 0)
    {
        const auto ratio = width / height;
        if(std::isfinite(ratio) && ratio > 0)
        {
            return ratio;
        }
    }
    return {};
}

std::string svg_number(float value)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if(result.ec != std::errc{})
    {
        throw std::runtime_error("Failed to format SVG dimensions");
    }
    return {buffer, result.ptr};
}

} // namespace

struct image_source::data
{
    image_intrinsics intrinsic;
    std::shared_ptr<const bitmap> raster;
    std::unique_ptr<lunasvg::Document> svg;
    size reference_size;
    std::mutex mutex;
    using cache_key = std::tuple<int, int, float, float>;
    std::map<cache_key, std::shared_ptr<const bitmap>> cache;
    cache_key last_size{};
    std::weak_ptr<const bitmap> last_image;
    std::size_t cache_bytes = 0;
    std::size_t cache_budget = 16 * 1024 * 1024;
};

image_source::image_source(std::shared_ptr<data> value)
    : m_data(std::move(value))
{
}

image_source::image_source(bitmap value)
    : m_data(std::make_shared<data>())
{
    validate_bitmap(value);
    m_data->intrinsic = {
        static_cast<float>(value.width),
        static_cast<float>(value.height),
        static_cast<float>(value.width) / value.height
    };
    m_data->raster = std::make_shared<const bitmap>(std::move(value));
}

image_source::data& image_source::state() const
{
    if(!m_data)
    {
        throw std::logic_error("Image source has been moved from");
    }
    return *m_data;
}

image_source image_source::from_bitmap(bitmap value)
{
    return image_source{std::move(value)};
}

image_source image_source::from_file(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if(!input)
    {
        throw std::runtime_error("Failed to open image file");
    }
    const auto length = input.tellg();
    if(length <= 0 || length > std::numeric_limits<int>::max())
    {
        throw std::runtime_error("Image file is empty or exceeds decoder input limits");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
    {
        throw std::runtime_error("Failed to read image file");
    }
    return from_bytes(bytes);
}

image_source image_source::from_bytes(std::span<const std::uint8_t> bytes)
{
    if(bytes.empty())
    {
        throw std::invalid_argument("Encoded image must not be empty");
    }
    if(bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::length_error("Image exceeds decoder input limits");
    }
    lunasvg::Bitmap decoded{plutovg_surface_load_from_image_data(bytes.data(), static_cast<int>(bytes.size()))};
    if(!decoded.isNull())
    {
        auto value = std::make_shared<data>();
        value->raster = convert(std::move(decoded));
        value->intrinsic = {
            static_cast<float>(value->raster->width),
            static_cast<float>(value->raster->height),
            static_cast<float>(value->raster->width) / value->raster->height
        };
        return image_source{std::move(value)};
    }
    return from_svg({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
}

image_source image_source::from_svg(std::string_view text)
{
    if(text.empty())
    {
        throw std::invalid_argument("SVG text must not be empty");
    }
    auto value = std::make_shared<data>();
    // LunaSVG's parsed document owns all attributes and text.
    value->svg = lunasvg::Document::loadFromData(text.data(), text.size());
    if(!value->svg)
    {
        throw std::runtime_error("Unsupported or invalid image data");
    }

    auto root = value->svg->documentElement();
    auto& intrinsic = value->intrinsic;
    intrinsic.width = dimension(root.getAttribute("width"));
    intrinsic.height = dimension(root.getAttribute("height"));
    intrinsic.aspect_ratio = viewbox_ratio(root.getAttribute("viewBox"));
    if(intrinsic.width && intrinsic.height)
    {
        const auto ratio = *intrinsic.width / *intrinsic.height;
        if(std::isfinite(ratio) && ratio > 0)
        {
            intrinsic.aspect_ratio = ratio;
        }
    }

    // Dimensionless images use a 300 x 150 default object box, fitted to a
    // viewBox ratio when present. This reference viewport is independent of DPI.
    float width = intrinsic.width.value_or(300);
    float height = intrinsic.height.value_or(150);
    if(intrinsic.aspect_ratio)
    {
        const auto ratio = *intrinsic.aspect_ratio;
        if(intrinsic.width && !intrinsic.height)
        {
            height = width / ratio;
        }
        else if(intrinsic.height && !intrinsic.width)
        {
            width = height * ratio;
        }
        else if(!intrinsic.width && !intrinsic.height)
        {
            if(ratio >= 2)
            {
                height = width / ratio;
            }
            else
            {
                width = height * ratio;
            }
        }
    }
    if(!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
    {
        throw std::runtime_error("SVG intrinsic dimensions are not representable");
    }
    root.setAttribute("width", svg_number(width));
    root.setAttribute("height", svg_number(height));
    value->reference_size = {width, height};
    return image_source{std::move(value)};
}

image_intrinsics image_source::intrinsic() const
{
    return state().intrinsic;
}

std::shared_ptr<const void> image_source::identity() const
{
    (void)state();
    return m_data;
}

std::shared_ptr<const bitmap> image_source::prepare(size logical_size, float scale) const
{
    auto& value = state();
    const auto width = physical_dimension(logical_size.width, scale);
    const auto height = physical_dimension(logical_size.height, scale);
    (void)pixel_bytes(width, height);
    if(value.raster)
    {
        return value.raster;
    }

    const std::lock_guard lock{value.mutex};
    auto reference = value.reference_size;
    if(!value.intrinsic.aspect_ratio)
    {
        // Without an intrinsic ratio, relative SVG lengths resolve against the
        // actual logical viewport. DPI must never change their logical size.
        if(!value.intrinsic.width)
        {
            reference.width = logical_size.width;
        }
        if(!value.intrinsic.height)
        {
            reference.height = logical_size.height;
        }
    }
    const auto key = data::cache_key{width, height, reference.width, reference.height};
    if(const auto found = value.cache.find(key); found != value.cache.end())
    {
        return found->second;
    }
    if(value.last_size == key)
    {
        if(auto image = value.last_image.lock())
        {
            return image;
        }
    }

    auto root = value.svg->documentElement();
    root.setAttribute("width", svg_number(reference.width));
    root.setAttribute("height", svg_number(reference.height));
    auto image = convert(value.svg->renderToBitmap(width, height));
    constexpr auto overhead = sizeof(bitmap) + 128;
    if(value.cache_budget >= overhead && image->pixels.size() <= value.cache_budget - overhead)
    {
        const auto bytes = image->pixels.size() + overhead;
        if(value.cache_bytes > value.cache_budget - bytes)
        {
            value.cache.clear();
            value.cache_bytes = 0;
        }
        value.cache.emplace(key, image);
        value.cache_bytes += bytes;
    }
    value.last_size = key;
    value.last_image = image;
    return image;
}

void image_source::clear_cache() const
{
    auto& value = state();
    const std::lock_guard lock{value.mutex};
    value.cache.clear();
    value.last_image.reset();
    value.cache_bytes = 0;
}

void image_source::set_cache_budget(std::size_t bytes) const
{
    auto& value = state();
    const std::lock_guard lock{value.mutex};
    value.cache_budget = bytes;
    value.cache.clear();
    value.last_image.reset();
    value.cache_bytes = 0;
}

} // namespace centui
