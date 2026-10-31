#include "layout_data.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace centypo::details
{

rect caret_bounds(const layout_data& layout, text_position position)
{
    rect result{0, 0, 1, layout.line_height};
    std::size_t distance = std::numeric_limits<std::size_t>::max();
    bool exact_affinity = false;

    for(const auto& caret: layout.carets)
    {
        const auto delta =
            caret.offset > position.offset ? caret.offset - position.offset : position.offset - caret.offset;
        const bool affinity = caret.affinity == position.affinity;
        if(delta < distance || (delta == distance && affinity && !exact_affinity))
        {
            distance = delta;
            exact_affinity = affinity;
            result.x = caret.position.x;
            result.y = caret.position.y;
            if(caret.line < layout.lines.size() && layout.lines[caret.line].height > 0)
            {
                result.height = layout.lines[caret.line].height;
            }
        }
    }

    return result;
}

rect caret_bounds(const layout_data& layout, std::size_t offset)
{
    return caret_bounds(layout, text_position{offset});
}

text_position hit_text(const layout_data& layout, vec2 point)
{
    text_position result;
    float best_y = std::numeric_limits<float>::infinity();
    float best_x = std::numeric_limits<float>::infinity();

    for(const auto& caret: layout.carets)
    {
        const auto height = caret.line < layout.lines.size() && layout.lines[caret.line].height > 0
                                ? layout.lines[caret.line].height
                                : layout.line_height;
        const auto dy = std::abs(point.y - caret.position.y - height / 2);
        const auto dx = std::abs(point.x - caret.position.x);
        if(dy < best_y || (dy == best_y && dx < best_x))
        {
            best_y = dy;
            best_x = dx;
            result = {caret.offset, caret.affinity};
        }
    }

    return result;
}

std::size_t text_hit(const layout_data& layout, vec2 point)
{
    return hit_text(layout, point).offset;
}

text_position move_visual(const layout_data& layout, text_position position, bool forward)
{
    auto stops = layout.carets;
    std::ranges::sort(
        stops,
        [](const auto& a, const auto& b)
        {
            if(a.line != b.line)
            {
                return a.line < b.line;
            }

            if(a.position.x != b.position.x)
            {
                return a.position.x < b.position.x;
            }

            return a.offset < b.offset;
        }
    );
    stops.erase(
        std::unique(
            stops.begin(),
            stops.end(),
            [](const auto& a, const auto& b)
            {
                return a.offset == b.offset && a.position == b.position;
            }
        ),
        stops.end()
    );

    if(stops.empty())
    {
        return position;
    }

    const auto geometry = caret_bounds(layout, position);
    auto current = std::find_if(
        stops.begin(),
        stops.end(),
        [&](const auto& stop)
        {
            return stop.offset == position.offset && stop.position.x == geometry.x && stop.position.y == geometry.y;
        }
    );
    if(current == stops.end())
    {
        return position;
    }

    if(forward && current + 1 != stops.end())
    {
        ++current;
    }
    else if(!forward && current != stops.begin())
    {
        --current;
    }

    return {current->offset, current->affinity};
}

[[nodiscard]] std::vector<rect> selection_bounds(const layout_data& layout, std::size_t begin, std::size_t end)
{
    if(begin > end)
    {
        std::swap(begin, end);
    }

    std::vector<rect> result;
    if(begin == end)
    {
        return result;
    }

    for(const auto& cell: layout.selection_cells)
    {
        if(cell.begin < end && cell.end > begin && cell.bounds.width > 0)
        {
            result.push_back(cell.bounds);
        }
    }

    std::ranges::sort(
        result,
        [](rect a, rect b)
        {
            return a.y == b.y ? a.x < b.x : a.y < b.y;
        }
    );

    std::vector<rect> merged;
    for(const auto box: result)
    {
        if(!merged.empty() && merged.back().y == box.y && merged.back().height == box.height &&
           std::abs(merged.back().x + merged.back().width - box.x) < 0.01f)
        {
            merged.back().width = box.x + box.width - merged.back().x;
        }
        else
        {
            merged.push_back(box);
        }
    }

    return merged;
}

} // namespace centypo::details
