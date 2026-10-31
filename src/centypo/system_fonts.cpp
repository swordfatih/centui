#include "font_collection.hpp"
#include "unicode_analysis.hpp"

#include <cstdlib>
#include <fstream>
#include <optional>
#include <set>

#if !defined(CENTYPO_DISABLE_SYSTEM_FONTS) && !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite_2.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreText/CoreText.h>
#elif !defined(__ANDROID__)
#include <fontconfig/fontconfig.h>
#endif
#endif

namespace centypo::details
{
namespace
{

#if !defined(CENTYPO_DISABLE_SYSTEM_FONTS) && !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
struct com_deleter
{
    template <class T>
    void operator()(T* value) const noexcept
    {
        value->Release();
    }
};

template <class T>
using com_owner = std::unique_ptr<T, com_deleter>;

std::string utf8(std::wstring_view wide)
{
    const auto count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if(count == 0 && !wide.empty())
    {
        throw std::runtime_error("Invalid system font name");
    }

    std::string result(count, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        result.data(),
        count,
        nullptr,
        nullptr
    );

    return result;
}

std::wstring wide(std::string_view text)
{
    const auto size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');

    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);

    return result;
}

std::optional<std::string> family_name(IDWriteFontFamily& family)
{
    IDWriteLocalizedStrings* raw_names = nullptr;
    if(FAILED(family.GetFamilyNames(&raw_names)))
    {
        return std::nullopt;
    }

    com_owner<IDWriteLocalizedStrings> names{raw_names};
    UINT32 index = 0;
    BOOL found = FALSE;

    names->FindLocaleName(L"en-us", &index, &found);
    if(!found)
    {
        index = 0;
    }

    UINT32 length = 0;
    if(FAILED(names->GetStringLength(index, &length)))
    {
        return std::nullopt;
    }

    std::wstring name(length + 1, L'\0');
    if(FAILED(names->GetString(index, name.data(), length + 1)))
    {
        return std::nullopt;
    }

    name.resize(length);

    return utf8(name);
}

std::optional<std::string> family_name(IDWriteFont& font)
{
    IDWriteFontFamily* raw_family = nullptr;
    if(FAILED(font.GetFontFamily(&raw_family)))
    {
        return std::nullopt;
    }

    com_owner<IDWriteFontFamily> family{raw_family};

    return family_name(*family);
}

std::optional<std::filesystem::path> font_path(IDWriteFontFace& face)
{
    UINT32 file_count = 0;
    if(FAILED(face.GetFiles(&file_count, nullptr)) || file_count != 1)
    {
        return std::nullopt;
    }

    IDWriteFontFile* raw_file = nullptr;
    if(FAILED(face.GetFiles(&file_count, &raw_file)))
    {
        return std::nullopt;
    }

    com_owner<IDWriteFontFile> file{raw_file};
    IDWriteFontFileLoader* raw_loader = nullptr;
    if(FAILED(file->GetLoader(&raw_loader)))
    {
        return std::nullopt;
    }

    com_owner<IDWriteFontFileLoader> loader{raw_loader};
    IDWriteLocalFontFileLoader* raw_local = nullptr;
    const auto status =
        loader->QueryInterface(__uuidof(IDWriteLocalFontFileLoader), reinterpret_cast<void**>(&raw_local));
    if(FAILED(status))
    {
        return std::nullopt;
    }

    com_owner<IDWriteLocalFontFileLoader> local{raw_local};
    const void* key = nullptr;
    UINT32 key_size = 0;
    UINT32 path_size = 0;

    if(FAILED(file->GetReferenceKey(&key, &key_size)) ||
       FAILED(local->GetFilePathLengthFromKey(key, key_size, &path_size)))
    {
        return std::nullopt;
    }

    std::wstring path(path_size + 1, L'\0');
    if(FAILED(local->GetFilePathFromKey(key, key_size, path.data(), path_size + 1)))
    {
        return std::nullopt;
    }

    path.resize(path_size);

    return std::filesystem::path{path};
}

font_slant from_dwrite(DWRITE_FONT_STYLE style)
{
    switch(style)
    {
    case DWRITE_FONT_STYLE_ITALIC:
        return font_slant::italic;

    case DWRITE_FONT_STYLE_OBLIQUE:
        return font_slant::oblique;

    default:
        return font_slant::normal;
    }
}

DWRITE_FONT_STYLE to_dwrite(font_slant slant)
{
    switch(slant)
    {
    case font_slant::italic:
        return DWRITE_FONT_STYLE_ITALIC;

    case font_slant::oblique:
        return DWRITE_FONT_STYLE_OBLIQUE;

    default:
        return DWRITE_FONT_STYLE_NORMAL;
    }
}

class analysis_source final : public IDWriteTextAnalysisSource
{
public:
    explicit analysis_source(std::string_view text, std::string_view language)
        : text(wide(text)),
          locale(wide(language.empty() ? "en-us" : language))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** output) override
    {
        if(!output)
        {
            return E_POINTER;
        }

        *output = nullptr;
        if(id != __uuidof(IUnknown) && id != __uuidof(IDWriteTextAnalysisSource))
        {
            return E_NOINTERFACE;
        }

        *output = static_cast<IDWriteTextAnalysisSource*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++references;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const auto left = --references;
        if(!left)
        {
            delete this;
        }

        return left;
    }

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 at, const WCHAR** value, UINT32* length) override
    {
        *length = at < text.size() ? static_cast<UINT32>(text.size() - at) : 0;
        *value = *length ? text.data() + at : nullptr;

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 at, const WCHAR** value, UINT32* length) override
    {
        *length = std::min<UINT32>(at, static_cast<UINT32>(text.size()));
        *value = *length ? text.data() : nullptr;

        return S_OK;
    }

    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override
    {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }

    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 at, UINT32* length, const WCHAR** value) override
    {
        *length = at < text.size() ? static_cast<UINT32>(text.size() - at) : 0;
        *value = locale.c_str();

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    GetNumberSubstitution(UINT32 at, UINT32* length, IDWriteNumberSubstitution** value) override
    {
        *length = at < text.size() ? static_cast<UINT32>(text.size() - at) : 0;
        *value = nullptr;

        return S_OK;
    }

    std::wstring text;

private:
    std::wstring locale;
    ULONG references = 1;
};
#elif defined(__APPLE__)
struct cf_deleter
{
    void operator()(const void* value) const noexcept
    {
        CFRelease(value);
    }
};

template <class T>
using cf_owner = std::unique_ptr<std::remove_pointer_t<T>, cf_deleter>;

std::string utf8(CFStringRef text)
{
    if(!text)
    {
        return {};
    }

    const auto size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(text), kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<std::size_t>(size), '\0');
    if(!CFStringGetCString(text, result.data(), size, kCFStringEncodingUTF8))
    {
        return {};
    }

    result.resize(std::char_traits<char>::length(result.c_str()));

    return result;
}
#elif defined(__ANDROID__)
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
#else
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
#endif
#endif

} // namespace

std::vector<font_source> system_fonts()
{
#if defined(CENTYPO_DISABLE_SYSTEM_FONTS) || defined(__EMSCRIPTEN__)
    return {};
#else
    std::vector<font_source> result;

#if defined(_WIN32)
    IDWriteFactory* raw_factory = nullptr;
    const auto status = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_ISOLATED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&raw_factory)
    );
    if(FAILED(status))
    {
        throw std::runtime_error("DirectWrite initialization failed");
    }

    com_owner<IDWriteFactory> factory{raw_factory};

    IDWriteFontCollection* raw_collection = nullptr;
    if(FAILED(factory->GetSystemFontCollection(&raw_collection, TRUE)))
    {
        throw std::runtime_error("DirectWrite font discovery failed");
    }

    com_owner<IDWriteFontCollection> collection{raw_collection};

    for(UINT32 family_index = 0; family_index < collection->GetFontFamilyCount(); ++family_index)
    {
        IDWriteFontFamily* raw_family = nullptr;
        if(FAILED(collection->GetFontFamily(family_index, &raw_family)))
        {
            continue;
        }

        com_owner<IDWriteFontFamily> family{raw_family};

        const auto name = family_name(*family);
        if(!name)
        {
            continue;
        }

        for(UINT32 font_index = 0; font_index < family->GetFontCount(); ++font_index)
        {
            IDWriteFont* raw_font = nullptr;
            if(FAILED(family->GetFont(font_index, &raw_font)))
            {
                continue;
            }

            com_owner<IDWriteFont> font{raw_font};
            if(font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
            {
                continue;
            }

            IDWriteFontFace* raw_face = nullptr;
            if(FAILED(font->CreateFontFace(&raw_face)))
            {
                continue;
            }

            com_owner<IDWriteFontFace> face{raw_face};

            const auto path = font_path(*face);
            if(!path)
            {
                continue;
            }

            result.push_back(
                {*path,
                 static_cast<long>(face->GetIndex()),
                 *name,
                 static_cast<int>(font->GetWeight()),
                 from_dwrite(font->GetStyle())}
            );
        }
    }
#elif defined(__APPLE__)
    cf_owner<CTFontCollectionRef> collection{CTFontCollectionCreateFromAvailableFonts(nullptr)};
    if(!collection)
    {
        throw std::runtime_error("CoreText font discovery failed");
    }

    cf_owner<CFArrayRef> descriptors{CTFontCollectionCreateMatchingFontDescriptors(collection.get())};
    if(!descriptors)
    {
        throw std::runtime_error("CoreText font discovery failed");
    }

    for(CFIndex i = 0; i < CFArrayGetCount(descriptors.get()); ++i)
    {
        const auto descriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(descriptors.get(), i));
        cf_owner<CFTypeRef> url{CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute)};
        cf_owner<CFTypeRef> name{CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute)};
        if(!url || !name || CFGetTypeID(url.get()) != CFURLGetTypeID())
        {
            continue;
        }

        cf_owner<CFStringRef> path{CFURLCopyFileSystemPath(static_cast<CFURLRef>(url.get()), kCFURLPOSIXPathStyle)};

        // Inspect collection faces instead of assuming TTC index zero.
        try
        {
            auto bytes = read_font(utf8(path.get()));
            auto first = std::make_shared<font_face>(bytes);

            for(long index = 0; index < first->face_count(); ++index)
            {
                auto face = index == 0 ? first : std::make_shared<font_face>(bytes, index);
                if(face->family() == utf8(static_cast<CFStringRef>(name.get())))
                {
                    result.push_back({utf8(path.get()), index, face->family(), face->weight(), face->slant()});
                }
            }
        }
        catch(const std::bad_alloc&)
        {
            throw;
        }
        catch(const std::exception&)
        {
            continue;
        }
    }
#elif defined(__ANDROID__)
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
#else
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
#endif

    std::set<std::pair<std::filesystem::path, long>> seen;
    std::erase_if(
        result,
        [&](const auto& source)
        {
            return !seen.emplace(source.path, source.index).second;
        }
    );

    return result;
#endif
}

std::map<std::string, std::vector<std::string>> system_font_aliases()
{
#if defined(CENTYPO_DISABLE_SYSTEM_FONTS)
    return {};
#elif defined(_WIN32)
    return {
        {"sans-serif", {"Segoe UI", "Arial"}},
        {"serif", {"Cambria", "Times New Roman"}},
        {"monospace", {"Consolas", "Courier New"}}
    };
#elif defined(__APPLE__)
    return {
        {"sans-serif", {"Helvetica Neue", "Helvetica"}},
        {"serif", {"Times", "Times New Roman"}},
        {"monospace", {"Menlo", "Monaco"}}
    };
#elif defined(__ANDROID__)
    return {
        {"sans-serif", {"sans-serif", "Roboto"}},
        {"serif", {"serif", "Noto Serif"}},
        {"monospace", {"monospace", "Droid Sans Mono"}}
    };
#elif !defined(__EMSCRIPTEN__)
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
#else
    return {};
#endif
}

std::vector<std::string> system_font_fallbacks(const font& request, std::string_view text, std::string_view language)
{
#if defined(CENTYPO_DISABLE_SYSTEM_FONTS) || defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    (void)request;
    (void)text;
    (void)language;

    return {};
#else
    std::vector<std::string> result;

#if defined(_WIN32)
    IDWriteFactory2* raw_factory = nullptr;
    const auto factory_status = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_ISOLATED,
        __uuidof(IDWriteFactory2),
        reinterpret_cast<IUnknown**>(&raw_factory)
    );
    if(FAILED(factory_status))
    {
        // Older Windows keeps catalog fallback ordering.
        return result;
    }

    com_owner<IDWriteFactory2> factory{raw_factory};
    IDWriteFontFallback* raw_fallback = nullptr;
    if(FAILED(factory->GetSystemFontFallback(&raw_fallback)))
    {
        return result;
    }

    com_owner<IDWriteFontFallback> fallback{raw_fallback};
    com_owner<analysis_source> source{new analysis_source(text, language)};

    auto family = request.families.front();
    const auto aliases = system_font_aliases();
    if(const auto at = aliases.find(family); at != aliases.end() && !at->second.empty())
    {
        family = at->second.front();
    }

    const auto base = wide(family);
    UINT32 cursor = 0;
    while(cursor < source->text.size())
    {
        UINT32 count = 0;
        IDWriteFont* raw_font = nullptr;
        FLOAT scale = 1;
        const auto status = fallback->MapCharacters(
            source.get(),
            cursor,
            static_cast<UINT32>(source->text.size()) - cursor,
            nullptr,
            base.c_str(),
            static_cast<DWRITE_FONT_WEIGHT>(request.weight),
            to_dwrite(request.slant),
            DWRITE_FONT_STRETCH_NORMAL,
            &count,
            &raw_font,
            &scale
        );
        com_owner<IDWriteFont> font{raw_font};
        if(FAILED(status) || count == 0)
        {
            break;
        }

        if(font)
        {
            if(auto name = family_name(*font))
            {
                result.push_back(std::move(*name));
            }
        }

        cursor += count;
    }
#elif defined(__APPLE__)
    const auto make_string = [](std::string_view value)
    {
        return CFStringCreateWithBytes(
            nullptr,
            reinterpret_cast<const UInt8*>(value.data()),
            value.size(),
            kCFStringEncodingUTF8,
            false
        );
    };

    cf_owner<CFStringRef> family{make_string(request.families.front())};
    cf_owner<CFStringRef> source{make_string(text)};
    cf_owner<CTFontRef> base{CTFontCreateWithName(family.get(), 16, nullptr)};
    if(!source || !base)
    {
        return result;
    }

    const auto boundaries = analyze_text(text, language);
    for(std::size_t i = 0; i + 1 < boundaries.graphemes.size(); ++i)
    {
        const auto begin = text_utf16_offset(text, boundaries.graphemes[i]);
        const auto end = text_utf16_offset(text, boundaries.graphemes[i + 1]);
        cf_owner<CTFontRef> selected{CTFontCreateForString(base.get(), source.get(), CFRangeMake(begin, end - begin))};
        if(selected)
        {
            cf_owner<CFStringRef> name{CTFontCopyFamilyName(selected.get())};
            result.push_back(utf8(name.get()));
        }
    }
#else
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
#endif

    return result;
#endif
}

} // namespace centypo::details
