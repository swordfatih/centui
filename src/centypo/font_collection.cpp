#include "font_collection.hpp"

#include <fstream>
#include <numeric>
#include <tuple>

namespace centypo
{

void font::validate() const
{
    if(families.empty() || weight < 1 || weight > 1000 || slant < font_slant::normal || slant > font_slant::oblique)
    {
        throw std::invalid_argument("Invalid font request");
    }

    for(const auto& family: families)
    {
        validate_text(family);
        if(family.empty())
        {
            throw std::invalid_argument("Font family cannot be empty");
        }
    }
}

namespace details
{
namespace
{

std::string folded(std::string value)
{
    for(auto& ch: value)
    {
        if(ch >= 'A' && ch <= 'Z')
        {
            ch += 'a' - 'A';
        }
    }

    return value;
}

std::shared_ptr<const std::vector<unsigned char>> snapshot(std::span<const unsigned char> bytes)
{
    return std::make_shared<const std::vector<unsigned char>>(bytes.begin(), bytes.end());
}

} // namespace

std::shared_ptr<const std::vector<unsigned char>> read_font(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    const auto end = stream ? static_cast<std::streamoff>(stream.tellg()) : -1;
    if(end <= 0 || static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max())
    {
        throw std::runtime_error("Cannot read font: " + path.string());
    }

    auto result = std::make_shared<std::vector<unsigned char>>(static_cast<std::size_t>(end));
    stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(result->data()), end))
    {
        throw std::runtime_error("Cannot read font: " + path.string());
    }

    return result;
}

font_collection::font_collection(bool discover_system)
    : m_discover_system(discover_system)
{
    if(discover_system)
    {
        for(auto source: system_fonts())
        {
            m_entries.push_back({std::move(source), {}, false, false});
        }
        m_aliases = system_font_aliases();
    }
}

std::shared_ptr<font_collection> font_collection::with_font(std::span<const std::byte> bytes) const
{
    const auto owned = snapshot({reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()});
    auto first = std::make_shared<font_face>(owned);

    std::vector<entry> staged;
    const auto count = first->face_count();
    for(long index = 0; index < count; ++index)
    {
        auto native = index == 0 ? first : std::make_shared<font_face>(owned, index);
        font_source source{{}, index, native->family(), native->weight(), native->slant()};
        staged.push_back({std::move(source), std::move(native), true, false});
    }

    auto next = std::make_shared<font_collection>(*this);
    next->m_entries.insert(next->m_entries.end(), staged.begin(), staged.end());

    return next;
}

std::shared_ptr<font_collection> font_collection::refreshed(bool discover_system) const
{
    auto next = std::make_shared<font_collection>(discover_system);
    for(const auto& entry: m_entries)
    {
        if(entry.application)
        {
            next->m_entries.push_back(entry);
        }
    }

    next->m_application_aliases = m_application_aliases;
    for(const auto& [alias, families]: m_application_aliases)
    {
        next->m_aliases[alias] = families;
    }

    return next;
}

std::shared_ptr<font_collection> font_collection::with_alias(std::string alias, std::vector<std::string> families) const
{
    validate_text(alias);
    if(alias != "sans-serif" && alias != "serif" && alias != "monospace")
    {
        throw std::invalid_argument("Only generic font aliases can be mapped");
    }

    font{families}.validate();
    for(const auto& family: families)
    {
        const auto key = folded(family);
        if(key == "sans-serif" || key == "serif" || key == "monospace")
        {
            throw std::invalid_argument("Font aliases must name concrete families");
        }
    }

    if(const auto found = m_aliases.find(alias); found != m_aliases.end() && found->second == families)
    {
        return {};
    }

    auto next = std::make_shared<font_collection>(*this);
    next->m_aliases[alias] = families;
    next->m_application_aliases[std::move(alias)] = std::move(families);

    return next;
}

std::vector<std::size_t> font_collection::resolve(
    const font& request,
    std::string_view text,
    std::string_view language
) const
{
    request.validate();

    std::vector<std::string> families;
    for(const auto& name: request.families)
    {
        const auto key = folded(name);
        const auto alias = m_aliases.find(key);
        if(alias != m_aliases.end())
        {
            for(const auto& family: alias->second)
            {
                families.push_back(folded(family));
            }
        }
        else
        {
            families.push_back(key);
        }
    }

    if(m_discover_system && !text.empty())
    {
        for(auto family: system_font_fallbacks(request, text, language))
        {
            const auto key = folded(std::move(family));
            if(std::find(families.begin(), families.end(), key) == families.end())
            {
                families.push_back(key);
            }
        }
    }

    std::vector<std::size_t> result(m_entries.size());
    std::iota(result.begin(), result.end(), 0);

    const auto rank = [&](std::size_t index)
    {
        const auto& entry = m_entries[index];
        const auto family = folded(entry.source.family);
        const auto found = std::find(families.begin(), families.end(), family);
        const auto requested = found != families.end();
        const auto family_rank = static_cast<std::size_t>(found - families.begin());
        const auto origin = entry.application ? 0 : 1;
        const auto slant = entry.source.slant == request.slant                                               ? 0
                           : entry.source.slant != font_slant::normal && request.slant != font_slant::normal ? 1
                                                                                                             : 2;
        const auto style = slant * 2000 + std::abs(entry.source.weight - request.weight);

        // Preserve platform fallback order outside requested families. Later
        // application registrations win equivalent face ties.
        return std::tuple{
            family_rank,
            requested ? origin : 0,
            requested ? style : 0,
            entry.application ? m_entries.size() - index : index
        };
    };

    std::vector<decltype(rank(0))> ranks;
    ranks.reserve(m_entries.size());
    for(std::size_t i = 0; i < m_entries.size(); ++i)
    {
        ranks.push_back(rank(i));
    }

    std::stable_sort(
        result.begin(),
        result.end(),
        [&](auto a, auto b)
        {
            return ranks[a] < ranks[b];
        }
    );

    return result;
}

std::shared_ptr<font_face> font_collection::face(std::size_t index, const font& request) const
{
    const auto& entry = m_entries.at(index);
    if(!entry.face && !entry.inaccessible)
    {
        try
        {
            entry.face = std::make_shared<font_face>(read_font(entry.source.path), entry.source.index);
        }
        catch(const std::bad_alloc&)
        {
            throw;
        }
        catch(const std::exception&)
        {
            entry.inaccessible = true;
        }
    }

    if(entry.face && entry.face->variable())
    {
        const auto key = std::tuple{index, request.weight, request.slant};
        if(const auto at = m_configurations.find(key); at != m_configurations.end())
        {
            return at->second;
        }

        auto configured = std::make_shared<font_face>(
            entry.face->bytes(),
            entry.face->index(),
            std::pair{request.weight, request.slant}
        );
        m_configurations.emplace(key, configured);

        return configured;
    }

    return entry.face;
}

std::vector<std::shared_ptr<const std::vector<unsigned char>>> font_collection::loaded_bytes() const
{
    std::vector<std::shared_ptr<const std::vector<unsigned char>>> result;
    for(const auto& entry: m_entries)
    {
        if(entry.face)
        {
            result.push_back(entry.face->bytes());
        }
    }

    return result;
}

} // namespace details
} // namespace centypo
