#include <centui/widgets/input.hpp>

namespace centui
{

input_widget::input_widget(std::string value, input_options options, centui::style_source appearance)
    : widget(std::move(appearance)),
      m_editor(options.multiline),
      m_multiline(options.multiline)
{
    set_text(std::move(value));
    set_interactive();
    set_focusable();

    set_on_change(std::move(options.on_change));
    set_on_submit(std::move(options.on_submit));
}

input_widget::~input_widget() = default;

std::string_view input_widget::value() const noexcept
{
    return m_editor.value();
}

void input_widget::set_text(std::string text)
{
    check_mutation();

    if(m_editor.set_text(std::move(text)))
    {
        m_preferred_x.reset();
        changed_layout();
    }
}

void input_widget::set_selection(std::size_t anchor, std::size_t caret)
{
    set_selection(centypo::text_position{anchor}, centypo::text_position{caret});
}

void input_widget::set_selection(centypo::text_position anchor, centypo::text_position caret)
{
    check_mutation();

    const bool composition = !m_editor.preedit().empty();
    if(!m_editor.select(anchor, caret))
    {
        return;
    }

    m_preferred_x.reset();
    m_reveal_caret = true;

    if(composition)
    {
        changed_layout();
    }
    else
    {
        invalidate_geometry();
    }
}

std::pair<centypo::text_position, centypo::text_position> input_widget::directional_selection() const noexcept
{
    return {m_editor.anchor(), m_editor.caret()};
}

std::pair<std::size_t, std::size_t> input_widget::selection() const noexcept
{
    return m_editor.selection();
}

void input_widget::set_on_change(centui::callback<std::string_view> callback)
{
    check_mutation();

    m_on_change.set(std::move(callback));
}

void input_widget::set_on_submit(centui::callback<> callback)
{
    check_mutation();

    m_on_submit.set(std::move(callback));
}

size input_widget::measure(const measure_context& context)
{
    const auto width = m_multiline && context.width.mode != constraint_mode::unconstrained
                           ? context.width.value
                           : std::numeric_limits<float>::infinity();
    prepare_text(context.typography, width);

    return {std::max(160.0f, m_layout->bounds().width), std::max(style().font_size * 1.5f, m_layout->bounds().height)};
}

baseline_function input_widget::baseline()
{
    return [this](size content)
    {
        prepare_text(typography(), content.width);

        return m_layout->baseline();
    };
}

void input_widget::on_geometry()
{
    m_editor.set_language(resolved_style().paragraph.value_or(centypo::text_options{}).language);
    refresh_text_geometry();
}

void input_widget::refresh_text_geometry()
{
    const auto area = content_bounds();
    prepare_text(typography(), area.width);

    const auto caret =
        m_layout->caret_bounds(centypo::text_position{m_editor.display_caret(), m_editor.caret().affinity});
    const bool reveal = m_reveal_caret;
    if(reveal)
    {
        m_scroll.x = std::max(caret.x + caret.width - area.width, std::min(m_scroll.x, caret.x));
        m_scroll.y = std::max(caret.y + caret.height - area.height, std::min(m_scroll.y, caret.y));
        m_reveal_caret = false;
    }

    m_scroll.x = std::clamp(m_scroll.x, 0.0f, std::max(0.0f, m_layout->bounds().width + 1 - area.width));
    m_scroll.y = std::clamp(m_scroll.y, 0.0f, std::max(0.0f, m_layout->bounds().height - area.height));

    if(reveal && focus_active())
    {
        request_reveal(
            {area.x - bounds().x + caret.x - m_scroll.x,
             area.y - bounds().y + caret.y - m_scroll.y,
             caret.width,
             caret.height}
        );
    }
}

void input_widget::paint(painter& output) const
{
    if(!m_layout)
    {
        return;
    }

    auto area = output.bounds();
    area.x -= m_scroll.x;
    area.y -= m_scroll.y;
    area.width = std::max(area.width, m_layout->bounds().width);
    area.height = std::max(area.height, m_layout->bounds().height);

    const auto [begin, end] = selection();
    auto highlight = output.foreground();
    highlight.a = static_cast<std::uint8_t>(highlight.a / 3);

    if(m_editor.preedit().empty())
    {
        for(auto box: m_layout->selection_bounds(begin, end))
        {
            box.x += area.x;
            box.y += area.y;
            output.rectangle(box, highlight);
        }
    }
    else
    {
        const auto selected = m_editor.composition_selection();
        for(auto box: m_layout->selection_bounds(selected.first, selected.second))
        {
            box.x += area.x;
            box.y += area.y;
            output.rectangle(box, highlight);
        }

        for(auto box: m_layout->selection_bounds(begin, begin + m_editor.preedit().size()))
        {
            box.x += area.x;
            box.y += area.y + box.height - 1;
            box.height = 1;
            output.rectangle(box, output.foreground());
        }
    }

    output.text(
        output.typography().rasterize(*m_layout, output.scale()),
        area,
        output.foreground(),
        true
    );

    if(focus_active())
    {
        auto caret =
            m_layout->caret_bounds(centypo::text_position{m_editor.display_caret(), m_editor.caret().affinity});
        caret.x += area.x;
        caret.y += area.y;
        output.rectangle(caret, output.foreground());
    }
}

std::optional<rect> input_widget::text_input_area() const
{
    if(!m_layout)
    {
        return content_bounds();
    }

    auto caret = m_layout->caret_bounds(centypo::text_position{m_editor.display_caret(), m_editor.caret().affinity});
    caret.x += content_bounds().x - m_scroll.x;
    caret.y += content_bounds().y - m_scroll.y;

    return caret;
}

void input_widget::on_focus_activity_changed(bool active)
{
    if(!active)
    {
        m_dragging = false;
        if(m_editor.cancel_composition())
        {
            changed_layout();
        }
    }
}

bool input_widget::on_event(const input_event& event)
{
    if(event.type == input_event_type::composition)
    {
        if(m_editor.compose(event.text, event.selection_start, event.selection_length))
        {
            changed_layout();
        }
        return true;
    }
    if(event.type == input_event_type::text_entered)
    {
        replace_selection(event.text, true);
        return true;
    }
    if(event.type == input_event_type::pointer_cancelled)
    {
        m_dragging = false;
        if(!focus_active() && m_editor.cancel_composition())
        {
            changed_layout();
        }
        return true;
    }
    if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
    {
        if(m_editor.cancel_composition())
        {
            changed_layout();
        }
        refresh_text_geometry();
        m_dragging = true;
        move_caret(hit(event.x, event.y), event.mods.shift);
        return true;
    }
    if(m_dragging &&
       (event.type == input_event_type::pointer_moved || event.type == input_event_type::pointer_released))
    {
        move_caret(hit(event.x, event.y), true);
        if(event.type == input_event_type::pointer_released)
        {
            m_dragging = false;
        }
        return true;
    }
    if(event.type == input_event_type::wheel && m_layout)
    {
        const auto previous = m_scroll;
        m_scroll.x = std::clamp(
            m_scroll.x + event.delta_x,
            0.0f,
            std::max(0.0f, m_layout->bounds().width + 1 - content_bounds().width)
        );
        m_scroll.y = std::clamp(
            m_scroll.y + event.delta_y,
            0.0f,
            std::max(0.0f, m_layout->bounds().height - content_bounds().height)
        );
        if(previous != m_scroll)
        {
            invalidate_geometry();
            return true;
        }
        return false;
    }
    if(event.type != input_event_type::key_pressed)
    {
        return false;
    }
    if(event.code == key::escape)
    {
        if(m_editor.cancel_composition())
        {
            changed_layout();
            return true;
        }
        return false;
    }
    const auto [begin, end] = selection();
    const bool command = event.mods.control || event.mods.system;
    if(command)
    {
        if(event.code == key::a)
        {
            set_selection(0, value().size());
            return true;
        }
        if(event.code == key::c || event.code == key::x)
        {
            if(begin != end && host().write_clipboard)
            {
                host().write_clipboard(value().substr(begin, end - begin));
                if(event.code == key::x)
                {
                    replace_selection({});
                }
            }
            return true;
        }
        if(event.code == key::v)
        {
            if(host().read_clipboard)
            {
                replace_selection(host().read_clipboard());
            }
            return true;
        }
        if(event.code == key::z || event.code == key::y)
        {
            const bool composition = !m_editor.preedit().empty();
            const bool changed = m_editor.undo(event.code == key::y || event.mods.shift);
            if(changed || composition)
            {
                changed_layout();
            }
            if(changed)
            {
                notify_change();
            }
            return true;
        }
    }
    switch(event.code)
    {
    case key::left:
    case key::right:
    {
        if(m_editor.cancel_composition())
        {
            changed_layout();
            refresh_text_geometry();
        }
        const bool forward = event.code == key::right;
        if(command)
        {
            move_caret({m_editor.adjacent(forward, true)}, event.mods.shift);
        }
        else if(!event.mods.shift && begin != end && m_layout)
        {
            const auto anchor = m_layout->caret_bounds(m_editor.anchor());
            const auto caret = m_layout->caret_bounds(m_editor.caret());
            const bool anchor_first = anchor.y == caret.y ? anchor.x < caret.x : anchor.y < caret.y;
            move_caret((forward == anchor_first) ? m_editor.caret() : m_editor.anchor(), false);
        }
        else if(m_layout)
        {
            move_caret(m_layout->move_visual(m_editor.caret(), forward), event.mods.shift);
        }
        else
        {
            move_caret({m_editor.adjacent(forward, false)}, event.mods.shift);
        }
        return true;
    }
    case key::up:
    case key::down:
    {
        if(m_editor.cancel_composition())
        {
            changed_layout();
            refresh_text_geometry();
        }
        if(m_multiline && m_layout)
        {
            const auto caret = m_layout->caret_bounds(m_editor.caret());
            const auto preferred = m_preferred_x.value_or(caret.x);
            move_caret(
                m_layout->hit_test({preferred, caret.y + caret.height * (event.code == key::up ? -0.5f : 1.5f)}),
                event.mods.shift
            );
            m_preferred_x = preferred;
        }
        return m_multiline;
    }
    case key::home:
    case key::end:
    {
        if(m_editor.cancel_composition())
        {
            changed_layout();
            refresh_text_geometry();
        }
        centypo::text_position next{event.code == key::home ? 0 : value().size()};
        if(m_layout && !command)
        {
            const auto caret = m_layout->caret_bounds(m_editor.caret());
            next = m_layout->hit_test(
                {event.code == key::home ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max(),
                 caret.y + caret.height / 2}
            );
        }
        move_caret(next, event.mods.shift);
        return true;
    }
    case key::backspace:
    case key::delete_:
    {
        const bool composition = !m_editor.preedit().empty();
        if(m_editor.erase(event.code == key::delete_, command))
        {
            changed_layout();
            notify_change();
        }
        else if(composition)
        {
            changed_layout();
        }
        return true;
    }
    case key::tab:
        if(m_multiline && !command && !event.mods.alt && !event.mods.shift)
        {
            replace_selection("\t");
            return true;
        }
        return false;
    case key::enter:
        if(m_multiline)
        {
            replace_selection("\n");
        }
        else if(!event.repeat)
        {
            m_on_submit(*this);
        }
        return true;
    default:
        return false;
    }
}

centypo::text_position input_widget::hit(float x, float y) const
{
    return m_layout ? m_layout->hit_test({x - content_bounds().x + m_scroll.x, y - content_bounds().y + m_scroll.y})
                    : centypo::text_position{};
}

void input_widget::move_caret(centypo::text_position next, bool extend)
{
    set_selection(extend ? m_editor.anchor() : next, next);
}

void input_widget::prepare_text(const centypo::engine& service, float width) const
{
    const auto appearance = resolved_style();
    auto options = appearance.paragraph.value_or(centypo::text_options{});
    if(!m_multiline)
    {
        options.wrap = centypo::text_wrap::none;
    }
    options.max_lines = 0;
    options.ellipsis = false;
    m_layout = service.layout(
        m_editor.display_text(),
        {.size = appearance.font_size,
         .width = width,
         .font = appearance.font.value_or(centypo::font{}),
         .paragraph = options,
         .alignment = appearance.text_align}
    );
}

void input_widget::changed_layout()
{
    m_layout.reset();
    m_reveal_caret = true;
    m_preferred_x.reset();
    invalidate_layout();
}

void input_widget::notify_change()
{
    const auto text = m_editor.value();
    m_on_change(*this, text);
}

void input_widget::replace_selection(std::string text, bool typing)
{
    const bool composition = !m_editor.preedit().empty();
    const auto before = directional_selection();
    const bool changed = m_editor.replace(std::move(text), typing);
    if(changed || composition)
    {
        changed_layout();
    }
    else if(before != directional_selection())
    {
        m_reveal_caret = true;
        invalidate_geometry();
    }
    if(changed)
    {
        notify_change();
    }
}

} // namespace centui
