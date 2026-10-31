#include "unicode_analysis.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

#include <centypo/unicode.hpp>
#include <graphemebreak.h>
#include <linebreak.h>
#include <wordbreak.h>

namespace centypo
{
namespace
{

struct scalar
{
    std::uint32_t value;
    std::size_t end;
};

scalar decode(std::string_view text, std::size_t begin)
{
    const auto first = static_cast<unsigned char>(text[begin]);
    const unsigned count = first < 0x80                     ? 1
                           : first >= 0xc2 && first <= 0xdf ? 2
                           : first >= 0xe0 && first <= 0xef ? 3
                           : first >= 0xf0 && first <= 0xf4 ? 4
                                                            : 0;
    if(count == 0 || count > text.size() - begin)
    {
        throw std::invalid_argument("Text requires valid UTF-8 without NUL");
    }

    std::uint32_t value = first & (count == 1 ? 0x7f : count == 2 ? 0x1f : count == 3 ? 0x0f : 0x07);
    for(unsigned i = 1; i < count; ++i)
    {
        const auto byte = static_cast<unsigned char>(text[begin + i]);
        if((byte & 0xc0) != 0x80)
        {
            throw std::invalid_argument("Text requires valid UTF-8 without NUL");
        }
        value = (value << 6) | (byte & 0x3f);
    }

    if(value == 0 || (count == 2 && value < 0x80) || (count == 3 && value < 0x800) || (count == 4 && value < 0x10000) ||
       value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
    {
        throw std::invalid_argument("Text requires valid UTF-8 without NUL");
    }

    return {value, begin + count};
}

} // namespace

void validate_text(std::string_view value)
{
    if(value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::length_error("Text exceeds native shaping limits");
    }

    for(std::size_t i = 0; i < value.size();)
    {
        i = decode(value, i).end;
    }
}

std::vector<details::unicode_scalar> details::decode_scalars(std::string_view text)
{
    validate_text(text);

    std::vector<unicode_scalar> result;
    for(std::size_t i = 0; i < text.size();)
    {
        const auto scalar = decode(text, i);
        result.push_back({scalar.value, i, scalar.end});
        i = scalar.end;
    }

    return result;
}

text_boundaries analyze_text(std::string_view value, std::string_view language)
{
    validate_text(value);
    validate_text(language);

    text_boundaries result{{0}, {0}, {0}, {}};
    if(value.empty())
    {
        return result;
    }

    const std::string locale{language};
    std::vector<char> graphemes(value.size()), words(value.size()), lines(value.size());
    const auto* bytes = reinterpret_cast<const utf8_t*>(value.data());
    const auto* lang = locale.empty() ? nullptr : locale.c_str();
    set_graphemebreaks_utf8(bytes, value.size(), lang, graphemes.data());
    set_wordbreaks_utf8(bytes, value.size(), lang, words.data());
    set_linebreaks_utf8(bytes, value.size(), lang, lines.data());

    // libunibreak stores the opportunity AFTER the indexed byte, including the
    // last byte of a multibyte scalar. Never expose continuation-byte offsets.
    for(std::size_t i = 0; i < value.size();)
    {
        const auto current = decode(value, i);
        const auto end = current.end;
        if(graphemes[end - 1] == GRAPHEMEBREAK_BREAK || end == value.size())
        {
            result.graphemes.push_back(end);
            if(lines[end - 1] == LINEBREAK_ALLOWBREAK || lines[end - 1] == LINEBREAK_MUSTBREAK || end == value.size())
            {
                result.lines.push_back(end);
            }
        }

        // Word and grapheme segmentation have independent rules (notably for
        // regional indicators separated by ignored characters such as ZWJ).
        if(words[end - 1] == WORDBREAK_BREAK || end == value.size())
        {
            result.words.push_back(end);
        }

        if(lines[end - 1] == LINEBREAK_MUSTBREAK &&
           (current.value == '\n' || current.value == '\r' || current.value == 0x85 || current.value == 0x2028 ||
            current.value == 0x2029 || current.value == 0x0b || current.value == 0x0c))
        {
            result.hard_breaks.push_back(end);
        }
        i = end;
    }

    return result;
}

std::size_t text_byte_offset(std::string_view value, std::size_t utf16_offset)
{
    validate_text(value);

    std::size_t units = 0;
    std::size_t i = 0;
    while(i < value.size())
    {
        const auto current = decode(value, i);
        const auto count = current.value > 0xffff ? 2u : 1u;
        if(units >= utf16_offset || utf16_offset - units < count)
        {
            break;
        }
        units += count;
        i = current.end;
    }

    return i;
}

std::size_t text_utf16_offset(std::string_view value, std::size_t byte_offset)
{
    validate_text(value);

    std::size_t units = 0;
    for(std::size_t i = 0; i < value.size();)
    {
        const auto current = decode(value, i);
        if(current.end > byte_offset)
        {
            break;
        }
        units += current.value > 0xffff ? 2 : 1;
        i = current.end;
    }

    return units;
}

std::size_t text_byte_offset_from_codepoints(std::string_view value, std::size_t count)
{
    validate_text(value);

    std::size_t offset = 0;
    while(count > 0 && offset < value.size())
    {
        offset = decode(value, offset).end;
        --count;
    }

    return offset;
}

} // namespace centypo
