#include <fstream>

#include <centui/document.hpp>
#include <centui/widgets.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Font constructors accept family arguments and own their names", "[font]")
{
    std::string primary = "Arial";
    const std::string_view fallback = "Inter";
    const centypo::font single{"Arial"};
    const centypo::font multiple{primary, fallback, "sans-serif"};
    primary = "changed";
    REQUIRE(single.families == std::vector<std::string>{"Arial"});
    REQUIRE(multiple.families == std::vector<std::string>{"Arial", "Inter", "sans-serif"});
    REQUIRE(single.weight == 400);
    REQUIRE(single.slant == centypo::font_slant::normal);
    const centypo::font copy{multiple};
    REQUIRE(copy == multiple);
    const centypo::font weighted{std::vector<std::string>{"Inter"}, 700, centypo::font_slant::italic};
    REQUIRE(weighted.weight == 700);
    REQUIRE(weighted.slant == centypo::font_slant::italic);
}

TEST_CASE("Font requests have ordinary defaults and validate before publication", "[font]")
{
    const centypo::font request;
    REQUIRE(request.families == std::vector<std::string>{"sans-serif"});
    REQUIRE(request.weight == 400);
    REQUIRE(request.slant == centypo::font_slant::normal);
    for(const auto& invalid:
        {centypo::font{{}},
         centypo::font{{""}},
         centypo::font{{"Inter"}, 0},
         centypo::font{{"Inter"}, 1001},
         centypo::font{{std::string("a\0b", 3)}}})
    {
        REQUIRE_THROWS_AS(invalid.validate(), std::invalid_argument);
    }
    centui::document document{centui::text{"Registered font"}, {.system_font_discovery = false}};
    REQUIRE_THROWS_AS(document.prepare({300, 100}), std::runtime_error);
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    REQUIRE_NOTHROW(document.prepare({300, 100}));
    const auto original = document.root()->style();
    REQUIRE_THROWS_AS(document.root()->patch_style({.font = centypo::font{{"Inter"}, -1}}), std::invalid_argument);
    REQUIRE(document.root()->style() == original);
}

TEST_CASE("Explicit aliases resolve registered fonts and preserve fractional measurement", "[font]")
{
    centui::document service_document{centui::document_options{.system_font_discovery = false}};
    service_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    service_document.set_font_alias("sans-serif", {"Inter"});
    service_document.set_font_alias("serif", {"Inter"});
    service_document.set_font_alias("monospace", {"Inter"});
    auto& service = service_document.typography();
    const auto sans = service.layout("Hello", {.size = 19.5f});
    const auto serif = service.layout("Hello", {.size = 19.5f, .width = 500, .font = centypo::font{"serif"}});
    const auto mono = service.layout("Hello", {.size = 19.5f, .width = 500, .font = centypo::font{"monospace"}});
    REQUIRE(sans.glyphs().front().font_id() == serif.glyphs().front().font_id());
    REQUIRE(sans.glyphs().front().font_id() == mono.glyphs().front().font_id());
    REQUIRE(sans.bounds().width > service.layout("Hello", {.size = 19}).bounds().width);
    REQUIRE(sans.bounds().width < service.layout("Hello", {.size = 20}).bounds().width);
    for(const auto size:
        {0.0f,
         -1.0f,
         std::numeric_limits<float>::infinity(),
         std::numeric_limits<float>::quiet_NaN(),
         std::numeric_limits<float>::max()})
    {
        REQUIRE_THROWS_AS(service.layout("Hello", {.size = size}), std::invalid_argument);
    }
}

TEST_CASE("Fonts inherit and equal requests are no-ops", "[font][style]")
{
    auto label = centui::text{"Inherited"};
    auto parent = centui::container{label};
    parent->patch_style({.font = centypo::font{{"Inter"}}});
    centui::document document{parent, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    REQUIRE(label->resolved_style().font == centypo::font{{"Inter"}});
    (void)document.prepare({200, 100});
    parent->patch_style({.font = centypo::font{{"Inter"}}});
    REQUIRE_FALSE(document.pending_changes().layout);
    label->patch_style({.font = centypo::font{{"serif"}}});
    REQUIRE(label->resolved_style().font == centypo::font{{"serif"}});
    label->patch_style({.font = std::optional<centypo::font>{}});
    REQUIRE(label->resolved_style().font == centypo::font{{"Inter"}});
    parent->patch_style({.font = centypo::font{{"monospace"}}});
    REQUIRE(document.pending_changes().layout);
}

TEST_CASE("Ancestor requests precede document defaults", "[font][style][inheritance]")
{
    auto label = centui::text{"Inheritance"};
    auto parent = centui::container{label};
    centui::document document{parent, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    document.set_style({.font = centypo::font{{"Inter"}}});
    parent->patch_style({.font = centypo::font{{"serif"}}});
    REQUIRE(label->resolved_style().font == centypo::font{{"serif"}});
    (void)document.prepare({300, 100});
    document.set_style({.font = centypo::font{{"monospace"}}});
    REQUIRE(label->resolved_style().font == centypo::font{{"serif"}});
    parent->patch_style({.font = std::optional<centypo::font>{}});
    REQUIRE(label->resolved_style().font == centypo::font{{"monospace"}});
    label->patch_style({.font = centypo::font{{"Inter"}}});
    REQUIRE(label->resolved_style().font == centypo::font{{"Inter"}});
}

TEST_CASE("Registration wins matching faces and failed replacements retain the generation", "[font][collection]")
{
    centui::document document{centui::text{"Registered"}, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const centypo::font request{{"Inter"}};
    const auto first =
        document.typography().rasterize(document.typography().layout("Retained", {.size = 20, .font = request}));
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const auto second = document.typography().layout("Retained", {.size = 20, .font = request});
    REQUIRE(first.glyphs().front().font_id() != second.glyphs().front().font_id());
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    const auto third = document.typography().layout("Retained", {.size = 20, .font = request});
    REQUIRE(second.glyphs().front().font_id() != third.glyphs().front().font_id());
    (void)document.prepare({300, 100});
    const auto generation = document.typography().revision();
    REQUIRE_THROWS(document.register_font(std::filesystem::path{"centui-font-does-not-exist.ttf"}));
    const std::byte garbage[]{std::byte{1}, std::byte{2}};
    REQUIRE_THROWS(document.register_font(garbage));
    REQUIRE(document.typography().revision() == generation);
    REQUIRE_FALSE(document.pending_changes().layout);
    document.refresh_fonts();
    REQUIRE(
        document.typography().layout("Retained", {.size = 20, .font = request}).glyphs().front().font_id() ==
        third.glyphs().front().font_id()
    );
    REQUIRE(first.glyphs().front().image);
}

TEST_CASE("Byte registration snapshots caller storage and retained layouts outlive documents", "[font][lifetime]")
{
    centypo::layout retained;
    {
        std::ifstream file(CENTUI_FONT_PATH, std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};
        REQUIRE_FALSE(bytes.empty());
        centui::document document{centui::document_options{.system_font_discovery = false}};
        document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
        document.register_font({reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()});
        std::fill(bytes.begin(), bytes.end(), 0);
        retained = document.typography().layout("Retained", {.size = 20, .width = 500, .font = centypo::font{"Inter"}});
    }
    centui::document other_document{centui::document_options{.system_font_discovery = false}};
    other_document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    auto& other = other_document.typography();
    const auto rendered = other.rasterize(retained);
    REQUIRE(rendered.glyphs().front().image);
    REQUIRE(rendered.glyphs().front().font_id() == retained.glyphs().front().font_id());
}

namespace
{

class collection_mutator : public centui::widget
{
public:
    centui::document* owner = nullptr;
    bool mutate = true;
    bool mutate_paint = false;

protected:
    centui::size measure(const centui::measure_context&) override
    {
        if(mutate)
        {
            owner->refresh_fonts();
        }
        return {100, 20};
    }

    void paint(centui::painter& output) const override
    {
        if(mutate_paint)
        {
            owner->register_font(std::filesystem::path{CENTUI_FONT_PATH});
        }
        output.rectangle({0, 0, 30, 20}, {255, 255, 255, 255});
    }
};

} // namespace

TEST_CASE("Font collection mutations are guarded and measurement failure retries", "[font][lifecycle]")
{
    auto value = centui::make_widget<collection_mutator>();
    // An intrinsically sized child exercises measurement; Yoga can skip the
    // measure callback for a root given exact viewport dimensions.
    centui::document document{centui::container{value}, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    value->owner = &document;
    const auto generation = document.typography().revision();
    REQUIRE_THROWS_AS(document.prepare({300, 100}), std::logic_error);
    const auto failed_measurements = document.statistics().measurement_calls;
    REQUIRE(failed_measurements > 0);
    REQUIRE(document.pending_changes().layout);
    REQUIRE(document.typography().revision() == generation);
    value->mutate = false;
    REQUIRE_NOTHROW(document.prepare({300, 100}));
    REQUIRE(document.statistics().measurement_calls > failed_measurements);
    const auto published = document.prepare({300, 100});
    value->mutate_paint = true;
    value->patch_style({.text_color = centui::states_color::all({200, 200, 200, 255})});
    REQUIRE_THROWS_AS(document.prepare({300, 100}), std::logic_error);
    REQUIRE(document.typography().revision() == generation);
    REQUIRE_FALSE(published.commands.empty());
    value->mutate_paint = false;
    REQUIRE_NOTHROW(document.prepare({300, 100}));
    document.set_font_alias("sans-serif", {"Noto Serif"});
    REQUIRE(document.pending_changes().layout);
    (void)document.prepare({300, 100});
    document.set_font_alias("sans-serif", {"Noto Serif"});
    REQUIRE_FALSE(document.pending_changes().layout);
}
