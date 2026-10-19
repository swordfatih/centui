module;

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

export module centui.layout_engine;

import centui.model;
import centui.layout;
import centui.style;
import centui.unit;
import centui.diagnostics;

export namespace centui::details
{

class layout_engine
{
    struct entry
    {
        centui::layout                   native;
        std::optional<centui::style>     applied_style;
        std::optional<rect> measured_bounds;
        bool                             links_valid = false;
    };

public:
    ~layout_engine()
    {
        // Yoga owners must release their children before any node is destroyed.
        for(const auto& current: _entries)
        {
            if(current)
            {
                current->native.clear_children();
            }
        }
    }

    void resize(std::size_t size)
    {
        _entries.resize(size);
    }

    void transfer_from(layout_engine& source, std::size_t from, std::size_t to) noexcept
    {
        _entries[to] = std::move(source._entries[from]);
    }

    bool apply_style(std::size_t index, const centui::style& style, preparation_diagnostics& diagnostics)
    {
        auto& current = get(index);
        if(current.applied_style && !style.changes_from(*current.applied_style).layout)
        {
            return false;
        }

        current.native.update(style);
        current.applied_style = style;
        diagnostics.record(&preparation_statistics::style_updates);
        return true;
    }

    bool measure(std::size_t index, rect bounds,
                 preparation_diagnostics& diagnostics)
    {
        auto& current = get(index);
        if(current.measured_bounds && current.measured_bounds->width == bounds.width &&
           current.measured_bounds->height == bounds.height)
        {
            return false;
        }

        current.native.set_measure([bounds](float, float)
        {
            return bounds;
        });
        current.measured_bounds = bounds;
        diagnostics.record(&preparation_statistics::measurements);
        return true;
    }

    void detach_children(std::size_t index) noexcept
    {
        if(_entries[index])
        {
            _entries[index]->native.clear_children();
            _entries[index]->links_valid = false;
        }
    }

    bool synchronize_children(std::size_t index, std::span<const node_id> children,
                              preparation_diagnostics& diagnostics)
    {
        auto& current = get(index);
        if(current.links_valid)
        {
            return false;
        }

        for(const auto child: children)
        {
            current.native.insert_child(get(child.index).native);
        }
        current.links_valid = true;
        diagnostics.record(&preparation_statistics::topology_syncs);
        return true;
    }

    void calculate(std::size_t root, int width, int height, preparation_diagnostics& diagnostics)
    {
        get(root).native.calculate(static_cast<float>(width), static_cast<float>(height));
        diagnostics.record(&preparation_statistics::layout_passes);
    }

    [[nodiscard]] rect bounds(std::size_t index) const
    {
        return _entries[index]->native.bounds();
    }

    void erase(std::size_t index) noexcept
    {
        _entries[index].reset();
    }

private:
    entry& get(std::size_t index)
    {
        if(!_entries[index])
        {
            _entries[index] = std::make_unique<entry>();
        }
        return *_entries[index];
    }

    std::vector<std::unique_ptr<entry>> _entries;
};

} // namespace centui::details
