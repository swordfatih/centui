#include "fonts.hpp"

#include <algorithm>
#include <optional>

#include <dwrite_2.h>
#include <windows.h>

namespace centypo::details::system
{
namespace
{

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

    return result;
}

std::map<std::string, std::vector<std::string>> native_font_provider::aliases() const
{
    return {
        {"sans-serif", {"Segoe UI", "Arial"}},
        {"serif", {"Cambria", "Times New Roman"}},
        {"monospace", {"Consolas", "Courier New"}}
    };
}

std::vector<std::string> native_font_provider::fallbacks(
    const font& request,
    std::string_view text,
    std::string_view language
) const
{
    std::vector<std::string> result;

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
    const auto aliases = this->aliases();
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

    return result;
}

} // namespace

std::shared_ptr<const font_provider> make_font_provider()
{
    return std::make_shared<native_font_provider>();
}

} // namespace centypo::details::system
