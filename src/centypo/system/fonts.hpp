#pragma once

#include "../font_source.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace centypo::details::system
{

/// Native font discovery. Unsupported operations return empty results.
class font_provider
{
public:
    virtual ~font_provider() = default;

    [[nodiscard]] virtual std::vector<font_source> fonts() const
    {
        return {};
    }

    [[nodiscard]] virtual std::map<std::string, std::vector<std::string>> aliases() const
    {
        return {};
    }

    [[nodiscard]] virtual std::vector<std::string> fallbacks(const font&, std::string_view, std::string_view) const
    {
        return {};
    }
};

/// CMake selects the implementation; disabled discovery uses the base provider.
[[nodiscard]] std::shared_ptr<const font_provider> make_font_provider();

} // namespace centypo::details::system
