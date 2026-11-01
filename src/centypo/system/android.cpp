#include "fonts.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>

namespace centypo::details::system
{
namespace
{

std::string attribute(std::string_view tag, std::string_view name)
{
    auto at = tag.find(std::string(name) + "=");
    if(at == std::string_view::npos)
    {
        return {};
    }

    at += name.size() + 1;
    if(at >= tag.size() || (tag[at] != '\'' && tag[at] != '"'))
    {
        return {};
    }

    const auto end = tag.find(tag[at], at + 1);

    return end == std::string_view::npos ? std::string{} : std::string(tag.substr(at + 1, end - at - 1));
}

class native_font_provider final : public font_provider
{
public:
    std::vector<font_source> fonts() const override;
    std::map<std::string, std::vector<std::string>> aliases() const override;
};

std::vector<font_source> native_font_provider::fonts() const
{
    std::vector<font_source> result;

    for(const auto* config: {"/system/etc/fonts.xml", "/product/etc/fonts_customization.xml"})
    {
        std::ifstream input(config);
        if(!input)
        {
            continue;
        }

        const std::string xml{std::istreambuf_iterator<char>(input), {}};
        std::size_t cursor = 0;

        while((cursor = xml.find("<family", cursor)) != std::string::npos)
        {
            const auto name_end = cursor + 7;
            if(name_end < xml.size() && std::string_view{" \t\r\n>"}.find(xml[name_end]) == std::string_view::npos)
            {
                cursor = name_end;
                continue;
            }
            const auto tag_end = xml.find('>', cursor);
            const auto end = xml.find("</family>", tag_end);
            if(tag_end == std::string::npos || end == std::string::npos)
            {
                break;
            }

            const auto family = attribute(std::string_view{xml}.substr(cursor, tag_end - cursor), "name");
            auto position = tag_end + 1;
            while((position = xml.find("<font ", position)) < end)
            {
                const auto font_end = xml.find('>', position);
                const auto close = xml.find("</font>", font_end);
                if(font_end == std::string::npos || close == std::string::npos || close > end)
                {
                    break;
                }

                const auto tag = std::string_view{xml}.substr(position, font_end - position);
                auto file = xml.substr(font_end + 1, close - font_end - 1);
                const auto first = file.find_first_not_of(" \t\r\n");
                const auto last = file.find_first_of("< \t\r\n", first);
                if(first != std::string::npos)
                {
                    file = file.substr(first, last == std::string::npos ? last : last - first);
                    auto path = std::filesystem::path("/system/fonts") / file;
                    if(!std::filesystem::exists(path))
                    {
                        path = std::filesystem::path("/product/fonts") / file;
                    }

                    const auto weight = attribute(tag, "weight");
                    const auto index = attribute(tag, "index");
                    result.push_back(
                        {path,
                         index.empty() ? 0 : std::strtol(index.c_str(), nullptr, 10),
                         family,
                         weight.empty() ? 400 : std::atoi(weight.c_str()),
                         attribute(tag, "style") == "italic" ? font_slant::italic : font_slant::normal}
                    );
                }

                position = close + 7;
            }

            cursor = end + 9;
        }
    }

    return result;
}

std::map<std::string, std::vector<std::string>> native_font_provider::aliases() const
{
    return {
        {"sans-serif", {"sans-serif", "Roboto"}},
        {"serif", {"serif", "Noto Serif"}},
        {"monospace", {"monospace", "Droid Sans Mono"}}
    };
}

} // namespace

std::shared_ptr<const font_provider> make_font_provider()
{
    return std::make_shared<native_font_provider>();
}

} // namespace centypo::details::system
