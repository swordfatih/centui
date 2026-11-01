#include <centui/widgets/selection.hpp>

namespace centui
{

selection_widget::selection_widget(
    std::vector<std::string> items,
    selection_options options,
    centui::style_source appearance
)
    : widget(std::move(appearance)),
      m_items(std::move(items))
{
    for(const auto& item: m_items)
    {
        centypo::validate_text(item);
    }
    set_interactive();
    set_focusable();
    set_selected(options.selected);
    set_on_change(std::move(options.on_change));
}

void selection_widget::set_items(std::vector<std::string> items)
{
    check_mutation();
    for(const auto& item: items)
    {
        centypo::validate_text(item);
    }
    if(items == m_items)
    {
        return;
    }
    m_items = std::move(items);
    m_text.clear();
    m_row_offsets.clear();
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

    if(m_selected == index)
    {
        return;
    }
    m_selected = index;
    invalidate_paint();
}

std::optional<std::size_t> selection_widget::selected() const noexcept
{
    return m_selected;
}

void selection_widget::set_on_change(centui::callback<std::size_t> callback)
{
    check_mutation();
    m_on_change.set(std::move(callback));
}

size selection_widget::measure(const measure_context& context)
{
    return prepare_rows(
        context.typography,
        context.width.mode == constraint_mode::unconstrained ? std::numeric_limits<float>::infinity()
                                                             : context.width.value
    );
}

size selection_widget::prepare_rows(const centypo::engine& service, float constraint) const
{
    const auto appearance = resolved_style();
    float width = 0;
    std::vector<float> offsets{0};
    std::vector<centypo::layout> layouts;
    layouts.reserve(m_items.size());

    for(std::size_t i = 0; i < m_items.size(); ++i)
    {
        auto layout = service.layout(
            m_items[i],
            {.size = appearance.font_size,
             .width = constraint,
             .font = appearance.font.value_or(centypo::font{}),
             .paragraph = appearance.paragraph.value_or(centypo::text_options{}),
             .alignment = appearance.text_align}
        );
        width = std::max(width, layout.bounds().width);
        offsets.push_back(offsets.back() + std::max(appearance.font_size * 1.6f, layout.bounds().height));
        layouts.push_back(std::move(layout));
    }

    m_text = std::move(layouts);
    m_row_offsets = std::move(offsets);
    return {width, m_row_offsets.back()};
}

baseline_function selection_widget::baseline()
{
    return [this](size content)
    {
        (void)prepare_rows(typography(), content.width);
        if(m_items.empty())
        {
            return 0.0f;
        }
        return m_text.front().baseline();
    };
}

void selection_widget::on_geometry()
{
    (void)prepare_rows(typography(), content_bounds().width);
}

void selection_widget::paint(painter& output) const
{
    auto row = output.bounds();
    if(m_row_offsets.size() != m_items.size() + 1)
    {
        throw std::logic_error("Selection rows must be prepared before painting");
    }

    for(std::size_t index = 0; index < m_items.size(); ++index)
    {
        row.y = m_row_offsets[index];
        row.height = m_row_offsets[index + 1] - row.y;
        if(m_selected == index)
        {
            auto tint = output.foreground();
            tint.a = static_cast<std::uint8_t>(tint.a / 4);
            output.rectangle(row, tint);
        }

        output.text(output.typography().rasterize(m_text[index], output.scale()), row, output.foreground());
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
        const auto y = event.y - content_bounds().y;
        if(m_row_offsets.size() == m_items.size() + 1 && y >= 0 && y < m_row_offsets.back())
        {
            next = static_cast<std::size_t>(
                std::upper_bound(m_row_offsets.begin(), m_row_offsets.end(), y) - m_row_offsets.begin() - 1
            );
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
        if(m_row_offsets.size() == m_items.size() + 1)
        {
            request_reveal(
                {content_bounds().x - bounds().x,
                 content_bounds().y - bounds().y + m_row_offsets[*next],
                 content_bounds().width,
                 m_row_offsets[*next + 1] - m_row_offsets[*next]}
            );
        }
        m_on_change(*this, *next);
    }

    return true;
}

} // namespace centui
