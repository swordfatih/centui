#include "text_analysis.hpp"

#include <SheenBidi/SheenBidi.h>

namespace centypo::details
{
namespace
{

template <class T, auto Release>
struct native_deleter
{
    void operator()(T* value) const noexcept
    {
        Release(value);
    }
};

template <class T, auto Release>
using native_owner = std::unique_ptr<std::remove_pointer_t<T>, native_deleter<std::remove_pointer_t<T>, Release>>;

} // namespace

struct text_analysis::data
{
    std::string text;
    native_owner<SBAlgorithmRef, SBAlgorithmRelease> algorithm;
    std::vector<native_owner<SBParagraphRef, SBParagraphRelease>> paragraphs;
    std::vector<script_run> scripts;
    bool default_rtl = false;
};

text_analysis::text_analysis(std::string_view text, std::string_view language, text_direction direction)
    : text_boundaries(analyze_text(text, language)),
      m_data(std::make_unique<data>())
{
    m_data->text = text;
    m_data->default_rtl = direction == text_direction::rtl;

    if(text.empty())
    {
        return;
    }

    const SBCodepointSequence sequence{SBStringEncodingUTF8, m_data->text.data(), m_data->text.size()};
    m_data->algorithm.reset(SBAlgorithmCreate(&sequence));
    if(!m_data->algorithm)
    {
        throw std::bad_alloc{};
    }

    const auto base = direction == text_direction::automatic ? SBLevelDefaultLTR
                      : direction == text_direction::rtl     ? 1
                                                             : 0;
    std::size_t cursor = 0;
    while(cursor < text.size())
    {
        native_owner<SBParagraphRef, SBParagraphRelease> paragraph{SBAlgorithmCreateParagraph(
            m_data->algorithm.get(),
            cursor,
            text.size() - cursor,
            static_cast<SBLevel>(base)
        )};
        if(!paragraph || SBParagraphGetLength(paragraph.get()) == 0)
        {
            throw std::runtime_error("Bidi paragraph analysis failed");
        }
        cursor += SBParagraphGetLength(paragraph.get());
        m_data->paragraphs.push_back(std::move(paragraph));
    }

    native_owner<SBScriptLocatorRef, SBScriptLocatorRelease> locator{SBScriptLocatorCreate()};
    if(!locator)
    {
        throw std::bad_alloc{};
    }

    SBScriptLocatorLoadCodepoints(locator.get(), &sequence);
    while(SBScriptLocatorMoveNext(locator.get()))
    {
        const auto& run = *SBScriptLocatorGetAgent(locator.get());
        m_data->scripts.push_back({run.offset, run.offset + run.length, SBScriptGetUnicodeTag(run.script)});
    }
}

text_analysis::~text_analysis() = default;

const std::vector<text_analysis::script_run>& text_analysis::scripts() const noexcept
{
    return m_data->scripts;
}

std::vector<text_analysis::bidi_run> text_analysis::line_runs(std::size_t begin, std::size_t end) const
{
    std::vector<bidi_run> result;
    if(begin == end)
    {
        return result;
    }

    for(const auto& paragraph: m_data->paragraphs)
    {
        const auto offset = SBParagraphGetOffset(paragraph.get()), length = SBParagraphGetLength(paragraph.get());
        if(begin < offset || begin >= offset + length)
        {
            continue;
        }

        if(end > offset + length)
        {
            throw std::logic_error("Line crosses a bidi paragraph");
        }

        native_owner<SBLineRef, SBLineRelease> line{SBParagraphCreateLine(paragraph.get(), begin, end - begin)};
        if(!line)
        {
            throw std::bad_alloc{};
        }

        const auto* runs = SBLineGetRunsPtr(line.get());
        for(std::size_t i = 0; i < SBLineGetRunCount(line.get()); ++i)
        {
            result.push_back({runs[i].offset, runs[i].length, runs[i].level});
        }

        return result;
    }

    throw std::out_of_range("Line is outside analyzed text");
}

bool text_analysis::rtl(std::size_t offset) const
{
    for(const auto& paragraph: m_data->paragraphs)
    {
        const auto begin = SBParagraphGetOffset(paragraph.get());
        if(offset >= begin && offset < begin + SBParagraphGetLength(paragraph.get()))
        {
            return (SBParagraphGetBaseLevel(paragraph.get()) & 1) != 0;
        }
    }

    return m_data->default_rtl;
}

std::size_t text_analysis::storage_bytes() const
{
    // Includes a conservative reservation for opaque native analysis arrays.
    return sizeof(*this) + sizeof(data) + m_data->text.capacity() * 12 +
           (graphemes.capacity() + words.capacity() + lines.capacity() + hard_breaks.capacity()) * sizeof(std::size_t) +
           m_data->scripts.capacity() * sizeof(script_run) + m_data->paragraphs.capacity() * 512;
}

} // namespace centypo::details
