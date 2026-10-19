module;

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

export module centui.painter;

import centui.model;
import centui.unit;
import centui.glyph;
import centui.draw;
import centui.diagnostics;

export namespace centui::details
{

struct paint_item
{
    std::size_t                index;
    std::optional<std::size_t> parent;
    rect                       bounds;
    const element_content*     content;
    const text_layout*         text;
};

class painter
{
    struct command_range
    {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

public:
    [[nodiscard]] bool valid() const noexcept
    {
        return _valid;
    }

    void invalidate() noexcept
    {
        _valid = false;
    }

    // Items arrive in preorder. Build privately, then publish a complete frame.
    void rebuild(std::span<const paint_item> items, std::size_t slots,
                 preparation_diagnostics& diagnostics)
    {
        _valid = false;
        std::vector<draw_command>  commands;
        std::vector<command_range> ranges(slots);
        for(const auto& item: items)
        {
            auto& range = ranges[item.index];
            range.begin = commands.size();
            append_element_commands(*item.content, item.bounds, item.text, commands);
            range.end = commands.size();
        }
        for(auto current = items.rbegin(); current != items.rend(); ++current)
        {
            if(current->parent)
            {
                auto& parent = ranges[*current->parent];
                parent.end = std::max(parent.end, ranges[current->index].end);
            }
        }

        _commands.swap(commands);
        _ranges.swap(ranges);
        _valid = true;
        diagnostics.record(&preparation_statistics::paint_builds);
    }

    [[nodiscard]] std::span<const draw_command> commands(std::size_t index) const
    {
        const auto range = _ranges[index];
        return std::span<const draw_command>{_commands}.subspan(range.begin, range.end - range.begin);
    }

private:
    std::vector<draw_command>  _commands;
    std::vector<command_range> _ranges;
    bool                       _valid = false;
};

} // namespace centui::details
