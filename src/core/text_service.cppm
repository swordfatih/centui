module;

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module centui.text_service;

import centui.glyph;
import centui.diagnostics;

export namespace centui::details
{

// Each slot retains only its current inputs and immutable prepared text.
// Wrapping must extend this key when constrained shaping is introduced.
class text_service
{
    struct entry
    {
        std::shared_ptr<const void>        face;
        std::string                        value;
        float                              size = 0;
        std::shared_ptr<const text_layout> output;
    };

public:
    void resize(std::size_t size)
    {
        _entries.resize(size);
    }

    void transfer_from(text_service& source, std::size_t from, std::size_t to) noexcept
    {
        _entries[to] = std::move(source._entries[from]);
    }

    const std::shared_ptr<const text_layout>& prepare(std::size_t index, font& face,
                                                      const std::shared_ptr<const void>& identity, std::string_view value, float size,
                                                      preparation_diagnostics& diagnostics)
    {
        if(!_entries[index])
        {
            _entries[index] = std::make_unique<entry>();
        }
        auto& current = *_entries[index];
        if(current.output && current.face == identity && current.size == size && current.value == value)
        {
            return current.output;
        }

        std::string source{value};
        auto        output = std::make_shared<const text_layout>(face.layout_text(source, size));

        current.output = std::move(output);
        current.face = identity;
        current.value.swap(source);
        current.size = size;
        diagnostics.record(&preparation_statistics::shapings);
        return current.output;
    }

    [[nodiscard]] const text_layout* get(std::size_t index) const noexcept
    {
        return _entries[index] ? _entries[index]->output.get() : nullptr;
    }

    void erase(std::size_t index) noexcept
    {
        _entries[index].reset();
    }

private:
    std::vector<std::unique_ptr<entry>> _entries;
};

} // namespace centui::details
