#pragma once

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <centypo/types.hpp>
#include <centypo/unicode.hpp>

namespace centypo
{

/// Committed and provisional editing state. It has no widget, renderer or host dependency.
class editor
{
public:
    explicit editor(bool multiline = false);

    const std::string& value() const noexcept
    {
        return m_value;
    }

    const std::string& preedit() const noexcept
    {
        return m_preedit;
    }

    text_position anchor() const noexcept
    {
        return m_anchor;
    }

    text_position caret() const noexcept
    {
        return m_caret;
    }

    std::pair<std::size_t, std::size_t> selection() const noexcept;

    std::size_t display_caret() const noexcept;
    std::string display_text() const;

    bool set_text(std::string text);
    bool select(text_position anchor, text_position caret);

    bool compose(std::string text, std::size_t caret, std::size_t selection_length = 0);
    std::pair<std::size_t, std::size_t> composition_selection() const noexcept;
    bool cancel_composition();

    bool replace(std::string text, bool typing = false);
    bool erase(bool forward, bool word);

    bool undo(bool redo);

    std::size_t adjacent(bool forward, bool word) const;

    void set_language(std::string language);

private:
    struct snapshot
    {
        std::string value;
        text_position anchor;
        text_position caret;
    };

    enum class edit_kind
    {
        none,
        typing,
        backward,
        forward
    };

    std::string normalize(std::string text) const;
    std::size_t boundary(std::size_t offset) const noexcept;

    bool commit(std::string replacement, std::size_t begin, std::size_t end, edit_kind kind);

    snapshot current() const;
    void end_group() noexcept;

    bool m_multiline;
    std::string m_value;

    std::string m_preedit;
    std::size_t m_preedit_caret = 0;
    std::size_t m_preedit_selection_end = 0;

    text_position m_anchor;
    text_position m_caret;

    std::string m_language;
    text_boundaries m_boundaries{{0}, {0}, {0}, {}};

    std::vector<snapshot> m_undo;
    std::vector<snapshot> m_redo;
    edit_kind m_group = edit_kind::none;
    std::chrono::steady_clock::time_point m_last_edit{};
};

} // namespace centypo
