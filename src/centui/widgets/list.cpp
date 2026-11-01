#include <limits>
#include <stdexcept>
#include <vector>

#include <centui/widgets/list.hpp>

namespace centui
{

namespace
{

float content_height(std::size_t count, float row_height)
{
    const auto height = static_cast<double>(count) * row_height;

    if(!std::isfinite(row_height) || row_height <= 0 || height > std::numeric_limits<float>::max())
    {
        throw std::invalid_argument("List row height must be positive and total height must be finite");
    }

    return static_cast<float>(height);
}

} // namespace

class list_widget::content_widget : public container_widget
{
public:
    using container_widget::container_widget;
    using widget::reorder_children;
};

list_widget::list_widget(row_factory factory, list_options options, centui::style_source appearance)
    : scroll_widget(std::move(appearance)),
      m_factory(std::move(factory)),
      m_options(options)
{
    if(!m_factory)
    {
        throw std::invalid_argument("A list requires a row factory");
    }

    centui::style content;
    content.width = percent(100);
    content.height = px(content_height(options.item_count, options.row_height));

    m_content = make_widget<content_widget>(content);
    append(m_content);
}

std::size_t list_widget::item_count() const noexcept
{
    return m_options.item_count;
}

void list_widget::check_factory_mutation() const
{
    check_mutation();

    if(m_building)
    {
        throw std::logic_error("A row factory must not modify its list");
    }
}

void list_widget::set_item_count(std::size_t count)
{
    check_factory_mutation();

    (void)content_height(count, m_options.row_height);

    if(count == m_options.item_count)
    {
        return;
    }

    m_options.item_count = count;

    if(m_scroll_target && *m_scroll_target >= count)
    {
        m_scroll_target.reset();
    }

    invalidate_geometry();
}

void list_widget::refresh()
{
    check_factory_mutation();

    m_refresh = true;

    invalidate_geometry();
}

void list_widget::scroll_to(std::size_t index)
{
    check_factory_mutation();

    if(index >= m_options.item_count)
    {
        throw std::out_of_range("List item is outside the data range");
    }

    m_scroll_target = index;

    invalidate_geometry();
}

std::pair<std::size_t, std::size_t> list_widget::rendered_range() const noexcept
{
    return m_range;
}

void list_widget::on_geometry()
{
    const auto total = content_height(m_options.item_count, m_options.row_height);

    if(m_content->style().height != px(total))
    {
        m_content->patch_style({.height = px(total)});

        return;
    }

    // The full-height content supplies the existing scrollbar's extent.
    scroll_widget::on_geometry();

    const auto area = content_bounds();

    if(m_scroll_target && area.height > 0)
    {
        const auto top = static_cast<float>(static_cast<double>(*m_scroll_target) * m_options.row_height);
        const auto bottom = top + m_options.row_height;
        auto next = offset();

        if(top < next.y)
        {
            next.y = top;
        }
        else if(bottom > next.y + area.height)
        {
            next.y = area.height < m_options.row_height ? top : bottom - area.height;
        }

        set_offset(next);
        m_scroll_target.reset();
    }

    const auto count = m_options.item_count;

    const auto bounded_index = [&](double value)
    {
        return value >= static_cast<double>(count) ? count : static_cast<std::size_t>(value);
    };

    const auto visible_first = bounded_index(std::floor(static_cast<double>(offset().y) / m_options.row_height));
    const auto visible_end =
        bounded_index(std::ceil((static_cast<double>(offset().y) + area.height) / m_options.row_height));
    const auto first =
        area.height > 0 && area.width > 0 ? visible_first - std::min(visible_first, m_options.overscan) : 0;
    const auto end =
        area.height > 0 && area.width > 0 ? visible_end + std::min(count - visible_end, m_options.overscan) : 0;

    // Stage factory calls before editing the mounted tree, so a throwing factory can be retried.
    std::map<std::size_t, handle<container_widget>> additions;
    m_building = true;

    try
    {
        for(auto index = first; index < end; ++index)
        {
            if(!m_refresh && m_rows.contains(index))
            {
                continue;
            }

            auto row = m_factory(index);

            centui::style slot;
            slot.position = position::absolute;
            slot.inset.top = px(static_cast<float>(static_cast<double>(index) * m_options.row_height));
            slot.inset.left = px(0);
            slot.width = percent(100);
            slot.height = px(m_options.row_height);
            slot.overflow = overflow::clip;

            additions.emplace(index, make_widget<container_widget>(slot, std::span{&row, std::size_t{1}}));
        }
    }
    catch(...)
    {
        m_building = false;

        throw;
    }

    m_building = false;

    for(auto it = m_rows.begin(); it != m_rows.end();)
    {
        if(m_refresh || it->first < first || it->first >= end)
        {
            m_content->remove(it->second);
            it = m_rows.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for(const auto& [index, row]: additions)
    {
        m_content->append(row);
        m_rows.emplace(index, row);
    }

    std::vector<handle<>> ordered;
    ordered.reserve(m_rows.size());

    for(const auto& [index, row]: m_rows)
    {
        ordered.push_back(row);
    }

    m_content->reorder_children(ordered);
    m_range = {first, end};
    m_refresh = false;
}

} // namespace centui
