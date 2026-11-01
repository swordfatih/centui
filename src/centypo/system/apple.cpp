#include "../font_face.hpp"
#include "../unicode_analysis.hpp"
#include "fonts.hpp"

#include <type_traits>

#include <CoreText/CoreText.h>

namespace centypo::details::system
{
namespace
{

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

    return result;
}

std::map<std::string, std::vector<std::string>> native_font_provider::aliases() const
{
    return {
        {"sans-serif", {"Helvetica Neue", "Helvetica"}},
        {"serif", {"Times", "Times New Roman"}},
        {"monospace", {"Menlo", "Monaco"}}
    };
}

std::vector<std::string> native_font_provider::fallbacks(
    const font& request,
    std::string_view text,
    std::string_view language
) const
{
    std::vector<std::string> result;

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

    return result;
}

} // namespace

std::shared_ptr<const font_provider> make_font_provider()
{
    return std::make_shared<native_font_provider>();
}

} // namespace centypo::details::system
