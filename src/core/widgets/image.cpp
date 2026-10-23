#include <centui/widgets/image.hpp>

namespace centui
{

image_widget::image_widget(bitmap value, centui::style appearance)
    : widget(std::move(appearance))
{
    set_image(std::move(value));
}

void image_widget::set_image(bitmap value)
{
    check_mutation();
    validate_bitmap(value);
    m_image = std::make_shared<const bitmap>(std::move(value));
    invalidate_layout();
}

size image_widget::measure(const measure_context&)
{
    return {static_cast<float>(m_image->width), static_cast<float>(m_image->height)};
}

void image_widget::paint(painter& output) const
{
    const auto area = output.bounds();
    const auto scale = std::min(area.width / m_image->width, area.height / m_image->height);
    const auto width = m_image->width * scale;
    const auto height = m_image->height * scale;
    output.image(m_image, {area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height});
}

} // namespace centui
