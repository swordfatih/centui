#include <algorithm>
#include <iterator>
#include <stdexcept>

#include <centypo/editor.hpp>

namespace centypo
{

editor::editor(bool multiline)
    : m_multiline(multiline)
{
}

std::pair<std::size_t, std::size_t> editor::selection() const noexcept
{
    return std::minmax(m_anchor.offset, m_caret.offset);
}

std::size_t editor::display_caret() const noexcept
{
    return m_preedit.empty() ? m_caret.offset : selection().first + m_preedit_caret;
}

std::string editor::display_text() const
{
    auto text = m_value;

    if(!m_preedit.empty())
    {
        const auto [begin, end] = selection();
        text.replace(begin, end - begin, m_preedit);
    }

    return text;
}

void editor::end_group() noexcept
{
    m_group = edit_kind::none;
}

editor::snapshot editor::current() const
{
    return {m_value, m_anchor, m_caret};
}

std::string editor::normalize(std::string value) const
{
    const auto boundaries = analyze_text(value, m_language);

    if(m_multiline)
    {
        return value;
    }

    std::string result;
    std::size_t begin = 0;

    for(const auto end: boundaries.hard_breaks)
    {
        auto content = end;
        if(value[end - 1] == '\n')
        {
            --content;
            if(content > begin && value[content - 1] == '\r')
            {
                --content;
            }
        }
        else if(value[end - 1] == '\r' || value[end - 1] == '\v' || value[end - 1] == '\f')
        {
            --content;
        }
        else if(end >= 2 && std::string_view{value}.substr(end - 2, 2) == "\xc2\x85")
        {
            content -= 2;
        }
        else
        {
            content -= 3;
        }

        result.append(value, begin, content - begin);
        result += ' ';
        begin = end;
    }

    result.append(value, begin, value.size() - begin);

    return result;
}

std::size_t editor::boundary(std::size_t offset) const noexcept
{
    const auto end = std::upper_bound(m_boundaries.graphemes.begin(), m_boundaries.graphemes.end(), offset);

    return end == m_boundaries.graphemes.begin() ? 0 : *std::prev(end);
}

bool editor::set_text(std::string value)
{
    auto next = normalize(std::move(value));
    auto boundaries = analyze_text(next, m_language);

    if(next == m_value && m_preedit.empty())
    {
        return false;
    }

    m_value.swap(next);
    m_boundaries = std::move(boundaries);
    m_anchor = m_caret = {m_value.size()};
    m_preedit.clear();
    m_undo.clear();
    m_redo.clear();
    end_group();

    return true;
}

bool editor::select(text_position anchor, text_position caret)
{
    if(anchor.affinity < text_affinity::downstream || anchor.affinity > text_affinity::upstream ||
       caret.affinity < text_affinity::downstream || caret.affinity > text_affinity::upstream)
    {
        throw std::invalid_argument("Invalid text affinity");
    }

    anchor.offset = boundary(std::min(anchor.offset, m_value.size()));
    caret.offset = boundary(std::min(caret.offset, m_value.size()));
    const bool changed = m_anchor != anchor || m_caret != caret || !m_preedit.empty();

    if(changed)
    {
        m_anchor = anchor;
        m_caret = caret;
        m_preedit.clear();
        end_group();
    }

    return changed;
}

bool editor::compose(std::string text, std::size_t caret, std::size_t selection_length)
{
    // Validate and map the host's source-byte caret before insertion policy can
    // change hard-break byte lengths in a single-line display.
    validate_text(text);

    caret = std::min(caret, text.size());
    while(caret > 0 && caret < text.size() && (static_cast<unsigned char>(text[caret]) & 0xc0) == 0x80)
    {
        --caret;
    }

    auto prefix = normalize(text.substr(0, caret));
    auto source_end = caret + std::min(selection_length, text.size() - caret);
    while(source_end > 0 && source_end < text.size() && (static_cast<unsigned char>(text[source_end]) & 0xc0) == 0x80)
    {
        --source_end;
    }

    auto selected_prefix = normalize(text.substr(0, source_end));
    auto next = normalize(std::move(text));
    const auto boundaries = analyze_text(next, m_language);

    const auto end = std::upper_bound(
        boundaries.graphemes.begin(),
        boundaries.graphemes.end(),
        std::min(prefix.size(), next.size())
    );
    const auto mapped = *std::prev(end);
    const auto selection_end = *std::prev(
        std::upper_bound(
            boundaries.graphemes.begin(),
            boundaries.graphemes.end(),
            std::min(selected_prefix.size(), next.size())
        )
    );

    if(next == m_preedit && mapped == m_preedit_caret && selection_end == m_preedit_selection_end)
    {
        return false;
    }

    m_preedit.swap(next);
    m_preedit_caret = mapped;
    m_preedit_selection_end = selection_end;
    end_group();

    return true;
}

std::pair<std::size_t, std::size_t> editor::composition_selection() const noexcept
{
    const auto begin = selection().first;

    return {begin + m_preedit_caret, begin + m_preedit_selection_end};
}

bool editor::cancel_composition()
{
    end_group();

    if(m_preedit.empty())
    {
        return false;
    }

    m_preedit.clear();

    return true;
}

bool editor::commit(std::string replacement, std::size_t begin, std::size_t end, edit_kind kind)
{
    auto next = m_value;
    next.replace(begin, end - begin, replacement);
    auto boundaries = analyze_text(next, m_language);
    // An edit can merge graphemes across either insertion edge. Keep the caret
    // after the resulting grapheme rather than inside it.
    const auto caret =
        *std::lower_bound(boundaries.graphemes.begin(), boundaries.graphemes.end(), begin + replacement.size());

    if(next == m_value)
    {
        if(begin != end || !replacement.empty())
        {
            m_anchor = m_caret = {caret};
        }

        m_preedit.clear();
        end_group();
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool group = kind != edit_kind::none && m_group == kind && m_anchor == m_caret &&
                       now - m_last_edit < std::chrono::seconds(1) && m_preedit.empty();

    if(!group)
    {
        m_undo.push_back(current());
        if(m_undo.size() > 128)
        {
            m_undo.erase(m_undo.begin());
        }
    }

    m_redo.clear();
    m_preedit.clear();
    m_value.swap(next);
    m_boundaries = std::move(boundaries);
    m_anchor = m_caret = {caret};
    m_group = kind;
    m_last_edit = now;

    return true;
}

bool editor::replace(std::string text, bool typing)
{
    auto normalized = normalize(std::move(text));
    const auto [begin, end] = selection();

    return commit(
        std::move(normalized),
        begin,
        end,
        typing && begin == end && m_preedit.empty() ? edit_kind::typing : edit_kind::none
    );
}

std::size_t editor::adjacent(bool forward, bool word) const
{
    const auto& stops = word ? m_boundaries.words : m_boundaries.graphemes;

    // Unicode word boundaries may fall inside a grapheme. Editing must keep
    // graphemes intact even when a word command encounters such a boundary.
    if(forward)
    {
        auto next = std::upper_bound(stops.begin(), stops.end(), m_caret.offset);
        while(next != stops.end() && word &&
              !std::binary_search(m_boundaries.graphemes.begin(), m_boundaries.graphemes.end(), *next))
        {
            ++next;
        }

        return next == stops.end() ? m_value.size() : *next;
    }

    auto next = std::lower_bound(stops.begin(), stops.end(), m_caret.offset);
    while(next != stops.begin())
    {
        --next;
        if(!word || std::binary_search(m_boundaries.graphemes.begin(), m_boundaries.graphemes.end(), *next))
        {
            return *next;
        }
    }

    return 0;
}

bool editor::erase(bool forward, bool word)
{
    auto [begin, end] = selection();

    if(begin == end)
    {
        const auto next = adjacent(forward, word);
        begin = std::min(next, m_caret.offset);
        end = std::max(next, m_caret.offset);
    }

    return commit({}, begin, end, word ? edit_kind::none : forward ? edit_kind::forward : edit_kind::backward);
}

bool editor::undo(bool redo)
{
    auto& source = redo ? m_redo : m_undo;
    auto& destination = redo ? m_undo : m_redo;

    if(source.empty())
    {
        cancel_composition();
        return false;
    }

    auto boundaries = analyze_text(source.back().value, m_language);

    destination.push_back(current());
    auto next = std::move(source.back());
    source.pop_back();

    m_value.swap(next.value);
    m_anchor = next.anchor;
    m_caret = next.caret;
    m_boundaries = std::move(boundaries);
    m_preedit.clear();
    end_group();

    return true;
}

void editor::set_language(std::string language)
{
    if(language == m_language)
    {
        return;
    }

    auto boundaries = analyze_text(m_value, language);

    m_language.swap(language);
    m_boundaries = std::move(boundaries);
    end_group();
}

} // namespace centypo
