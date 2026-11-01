#pragma once

#include "font_face.hpp"
#include "font_source.hpp"
#include "system/fonts.hpp"

#include <map>
#include <tuple>

namespace centypo::details
{

class font_collection
{
public:
    explicit font_collection(bool discover_system);

    [[nodiscard]] std::shared_ptr<font_collection> with_font(std::span<const std::byte> bytes) const;

    [[nodiscard]] std::shared_ptr<font_collection> refreshed(bool discover_system) const;

    [[nodiscard]] std::shared_ptr<font_collection> with_alias(
        std::string alias,
        std::vector<std::string> families
    ) const;

    [[nodiscard]] std::vector<std::size_t> resolve(
        const font& request,
        std::string_view text,
        std::string_view language
    ) const;

    [[nodiscard]] std::shared_ptr<font_face> face(std::size_t index, const font& request = {}) const;

    [[nodiscard]] std::vector<std::shared_ptr<const std::vector<unsigned char>>> loaded_bytes() const;

private:
    struct entry
    {
        font_source source;
        mutable std::shared_ptr<font_face> face;
        bool application = false;
        mutable bool inaccessible = false;
    };

    std::vector<entry> m_entries;
    std::map<std::string, std::vector<std::string>> m_aliases;
    std::map<std::string, std::vector<std::string>> m_application_aliases;

    bool m_discover_system = false;
    std::shared_ptr<const system::font_provider> m_system;

    mutable std::map<std::tuple<std::size_t, int, font_slant>, std::shared_ptr<font_face>> m_configurations;
};

} // namespace centypo::details
