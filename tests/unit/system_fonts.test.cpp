#include "system/fonts.hpp"

#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <centypo/engine.hpp>

namespace
{

class catalog_provider final : public centypo::details::system::font_provider
{
public:
    std::vector<centypo::details::font_source> fonts() const override
    {
        return {{"example.ttf", 0, "Example", 400, centypo::font_slant::normal}};
    }
};

} // namespace

TEST_CASE("Font providers inherit empty unsupported operations", "[font][system]")
{
    const auto provider = std::make_shared<const catalog_provider>();
    const centypo::details::system::font_provider& base = *provider;

    REQUIRE(base.fonts().size() == 1);
    REQUIRE(base.fonts().front().family == "Example");
    REQUIRE(base.aliases().empty());
    REQUIRE(base.fallbacks({}, "Example", "en").empty());

    const centypo::details::system::font_provider disabled;
    REQUIRE(disabled.fonts().empty());
    REQUIRE(disabled.aliases().empty());
    REQUIRE(disabled.fallbacks({}, "Example", "en").empty());
}

TEST_CASE("Registered fonts work with the configured system font provider", "[font][system]")
{
    const auto provider = centypo::details::system::make_font_provider();
    REQUIRE(provider);

    centypo::engine typography;
    typography.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    typography.set_font_alias("sans-serif", {"Inter"});
    const auto layout = typography.layout("Registered font");
    REQUIRE_FALSE(layout.glyphs().empty());

    typography.set_system_font_discovery(false);
    REQUIRE_FALSE(typography.layout("Registered font").glyphs().empty());
    typography.set_system_font_discovery(true);
    REQUIRE_FALSE(typography.layout("Registered font").glyphs().empty());
}
