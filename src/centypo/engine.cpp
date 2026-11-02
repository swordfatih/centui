#include "text_engine.hpp"

namespace centypo
{

font::font(std::vector<std::string> requested, int requested_weight, font_slant requested_slant)
    : families(std::move(requested)),
      weight(requested_weight),
      slant(requested_slant)
{
}

struct engine::data
{
    explicit data(engine_options options)
        : collection(std::make_shared<details::font_collection>(options.system_fonts)),
          discovery(options.system_fonts)
    {
        text.set_budgets(options.caches);
    }

    std::shared_ptr<details::font_collection> collection;
    details::text_engine text;
    bool discovery;
    std::uint64_t revision = 0;
};

engine::engine(engine_options options)
    : m_data(std::make_unique<data>(options))
{
}

engine::~engine() = default;

engine::engine(engine&&) noexcept = default;

engine& engine::operator=(engine&&) noexcept = default;

engine::data& engine::state() const
{
    if(!m_data)
    {
        throw std::logic_error("Cannot use a moved-from text engine");
    }

    return *m_data;
}

centypo::layout engine::layout(std::string_view value, const layout_options& options) const
{
    auto& current = state();

    return details::layout_access::make(current.text.layout(
        current.collection,
        value,
        options.font,
        options.size,
        options.width,
        options.paragraph,
        options.alignment
    ));
}

centypo::layout engine::rasterize(const centypo::layout& value, float scale) const
{
    if(!std::isfinite(scale) || scale <= 0)
    {
        throw std::invalid_argument("Raster scale must be finite and positive");
    }

    return details::layout_access::make(state().text.rasterize(details::layout_access::share(value), scale));
}

void engine::register_font(const std::filesystem::path& path)
{
    const auto bytes = details::read_font(path);

    register_font({reinterpret_cast<const std::byte*>(bytes->data()), bytes->size()});
}

void engine::register_font(std::span<const std::byte> bytes)
{
    auto& current = state();
    auto next = current.collection->with_font(bytes);

    current.collection = std::move(next);
    ++current.revision;
    current.text.clear();
}

void engine::set_font_alias(std::string alias, std::vector<std::string> families)
{
    auto& current = state();
    auto next = current.collection->with_alias(std::move(alias), std::move(families));

    if(next)
    {
        current.collection = std::move(next);
        ++current.revision;
        current.text.clear();
    }
}

void engine::refresh_fonts()
{
    auto& current = state();
    auto next = current.collection->refreshed(current.discovery);

    current.collection = std::move(next);
    ++current.revision;
    current.text.clear();
}

void engine::set_system_font_discovery(bool enabled)
{
    auto& current = state();

    if(enabled == current.discovery)
    {
        return;
    }

    auto next = current.collection->refreshed(enabled);

    current.collection = std::move(next);
    ++current.revision;
    current.discovery = enabled;
    current.text.clear();
}

void engine::clear_cache()
{
    state().text.clear();
}

void engine::set_cache_budgets(text_cache_budgets budgets)
{
    state().text.set_budgets(budgets);
}

text_cache_statistics engine::statistics() const
{
    const auto& current = state();

    return current.text.statistics(*current.collection);
}

std::uint64_t engine::revision() const
{
    return state().revision;
}

} // namespace centypo
