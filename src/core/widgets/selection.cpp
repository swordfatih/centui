#include <centui/widgets/selection.hpp>

namespace centui
{

selection_widget::selection_widget(std::vector<std::string> items, centui::style appearance)
    : widget(std::move(appearance)),
      m_items(std::move(items))
{
    set_interactive();
    set_focusable();
}

void selection_widget::set_items(std::vector<std::string> items)
{
    check_mutation();
    m_items = std::move(items);
    m_selected.reset();
    invalidate_layout();
}

void selection_widget::set_selected(std::optional<std::size_t> index)
{
    check_mutation();

    if(index && *index >= m_items.size())
    {
        throw std::out_of_range("Selection is outside the item list");
    }

    m_selected = index;
    invalidate_paint();
}

std::optional<std::size_t> selection_widget::selected() const noexcept
{
    return m_selected;
}

void selection_widget::set_on_change(std::function<void(std::size_t)> callback)
{
    check_mutation();
    m_on_change.set(std::move(callback));
}

size selection_widget::measure(const measure_context& context)
{
    const auto appearance = resolved_style();
    float width = 0;

    for(const auto& item: m_items)
    {
        width = std::max(width, context.text.prepare(item, appearance.font, appearance.font_size)->bounds.width);
    }

    return {width, row_height() * m_items.size()};
}

void selection_widget::paint(painter& output) const
{
    const auto appearance = resolved_style();
    auto row = output.bounds();
    row.height = row_height();

    for(std::size_t index = 0; index < m_items.size(); ++index)
    {
        if(m_selected == index)
        {
            auto tint = output.foreground();
            tint.a = static_cast<std::uint8_t>(tint.a / 4);
            output.rectangle(row, tint);
        }

        output.text(
            output.fonts().prepare(m_items[index], appearance.font, appearance.font_size, row.width),
            row,
            output.foreground()
        );
        row.y += row.height;
    }
}

bool selection_widget::on_event(const input_event& event)
{
    if(m_items.empty())
    {
        return false;
    }

    std::optional<std::size_t> next;

    if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
    {
        const auto row = std::floor((event.y - content_bounds().y) / row_height());

        if(row >= 0 && row < m_items.size())
        {
            next = static_cast<std::size_t>(row);
        }
    }
    else if(event.type == input_event_type::key_pressed)
    {
        switch(event.code)
        {
        case key::home:
        {
            next = 0;
            break;
        }

        case key::end:
        {
            next = m_items.size() - 1;
            break;
        }

        case key::down:
        {
            next = m_selected ? std::min(*m_selected + 1, m_items.size() - 1) : 0;
            break;
        }

        case key::up:
        {
            next = m_selected ? (*m_selected == 0 ? 0 : *m_selected - 1) : m_items.size() - 1;
            break;
        }

        default:
        {
            break;
        }
        }
    }

    if(!next)
    {
        return false;
    }

    if(m_selected != next)
    {
        set_selected(next);
        m_on_change(*next);
    }

    return true;
}

float selection_widget::row_height() const noexcept
{
    return style().font_size * 1.6f;
}

} // namespace centui
