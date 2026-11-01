#include "../unicode_analysis.hpp"
#include "fonts.hpp"

#include <fontconfig/fontconfig.h>

namespace centypo::details::system
{
namespace
{

using fc_config = std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)>;
using fc_pattern = std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)>;
using fc_font_set = std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)>;
using fc_charset = std::unique_ptr<FcCharSet, decltype(&FcCharSetDestroy)>;

font_slant from_fontconfig(int slant)
{
    switch(slant)
    {
    case FC_SLANT_ITALIC:
        return font_slant::italic;

    case FC_SLANT_OBLIQUE:
        return font_slant::oblique;

    default:
        return font_slant::normal;
    }
}

class native_font_provider final : public font_provider
{
public:
    std::vector<font_source> fonts() const override;
    std::map<std::string, std::vector<std::string>> aliases() const override;
    std::vector<std::string> fallbacks(
        const font& request,
        std::string_view text,
        std::string_view language
    ) const override;
};

std::vector<font_source> native_font_provider::fonts() const
{
    std::vector<font_source> result;

    fc_config config{FcInitLoadConfigAndFonts(), FcConfigDestroy};
    if(!config)
    {
        throw std::runtime_error("Fontconfig initialization failed");
    }

    fc_pattern pattern{FcPatternCreate(), FcPatternDestroy};
    if(!pattern)
    {
        throw std::bad_alloc{};
    }

    FcConfigSubstitute(config.get(), pattern.get(), FcMatchPattern);
    FcDefaultSubstitute(pattern.get());

    FcResult status;
    fc_font_set fonts{FcFontSort(config.get(), pattern.get(), FcFalse, nullptr, &status), FcFontSetDestroy};
    if(!fonts)
    {
        throw std::runtime_error("Fontconfig discovery failed");
    }

    for(int i = 0; i < fonts->nfont; ++i)
    {
        FcChar8* path = nullptr;
        FcChar8* family = nullptr;
        int index = 0;
        int weight = FC_WEIGHT_REGULAR;
        int slant = FC_SLANT_ROMAN;
        if(FcPatternGetString(fonts->fonts[i], FC_FILE, 0, &path) != FcResultMatch ||
           FcPatternGetString(fonts->fonts[i], FC_FAMILY, 0, &family) != FcResultMatch)
        {
            continue;
        }
        FcPatternGetInteger(fonts->fonts[i], FC_INDEX, 0, &index);
        FcPatternGetInteger(fonts->fonts[i], FC_WEIGHT, 0, &weight);
        FcPatternGetInteger(fonts->fonts[i], FC_SLANT, 0, &slant);

        result.push_back(
            {reinterpret_cast<const char*>(path),
             index,
             reinterpret_cast<const char*>(family),
             FcWeightToOpenType(weight),
             from_fontconfig(slant)}
        );
    }

    return result;
}

std::map<std::string, std::vector<std::string>> native_font_provider::aliases() const
{
    std::map<std::string, std::vector<std::string>> result;
    fc_config config{FcInitLoadConfigAndFonts(), FcConfigDestroy};
    if(!config)
    {
        throw std::runtime_error("Fontconfig initialization failed");
    }

    for(const auto* alias: {"sans-serif", "serif", "monospace"})
    {
        fc_pattern pattern{FcNameParse(reinterpret_cast<const FcChar8*>(alias)), FcPatternDestroy};
        if(!pattern)
        {
            throw std::bad_alloc{};
        }

        FcConfigSubstitute(config.get(), pattern.get(), FcMatchPattern);
        FcDefaultSubstitute(pattern.get());

        FcResult status;
        fc_pattern match{FcFontMatch(config.get(), pattern.get(), &status), FcPatternDestroy};

        FcChar8* name = nullptr;
        if(match && FcPatternGetString(match.get(), FC_FAMILY, 0, &name) == FcResultMatch)
        {
            result[alias].push_back(reinterpret_cast<const char*>(name));
        }
    }

    return result;
}

std::vector<std::string> native_font_provider::fallbacks(
    const font& request,
    std::string_view text,
    std::string_view language
) const
{
    std::vector<std::string> result;

    fc_config config{FcInitLoadConfigAndFonts(), FcConfigDestroy};
    fc_pattern pattern{FcPatternCreate(), FcPatternDestroy};
    fc_charset chars{FcCharSetCreate(), FcCharSetDestroy};
    if(!config || !pattern || !chars)
    {
        throw std::bad_alloc{};
    }

    for(const auto& family: request.families)
    {
        FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(family.c_str()));
    }

    for(const auto scalar: decode_scalars(text))
    {
        FcCharSetAddChar(chars.get(), scalar.value);
    }

    FcPatternAddCharSet(pattern.get(), FC_CHARSET, chars.get());
    if(!language.empty())
    {
        const std::string locale{language};
        FcPatternAddString(pattern.get(), FC_LANG, reinterpret_cast<const FcChar8*>(locale.c_str()));
    }

    FcConfigSubstitute(config.get(), pattern.get(), FcMatchPattern);
    FcDefaultSubstitute(pattern.get());

    FcResult status;
    fc_font_set fonts{FcFontSort(config.get(), pattern.get(), FcFalse, nullptr, &status), FcFontSetDestroy};
    if(fonts)
    {
        for(int i = 0; i < fonts->nfont; ++i)
        {
            FcChar8* name = nullptr;
            if(FcPatternGetString(fonts->fonts[i], FC_FAMILY, 0, &name) == FcResultMatch)
            {
                result.emplace_back(reinterpret_cast<const char*>(name));
            }
        }
    }

    return result;
}

} // namespace

std::shared_ptr<const font_provider> make_font_provider()
{
    return std::make_shared<native_font_provider>();
}

} // namespace centypo::details::system
