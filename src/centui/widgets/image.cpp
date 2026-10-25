#include <stdexcept>

#include <centui/widgets/image.hpp>

namespace centui
{
namespace
{

void validate(image_options options)
{
    if((options.fit != image_fit::contain && options.fit != image_fit::cover && options.fit != image_fit::fill) ||
       (options.sampling != image_sampling::linear && options.sampling != image_sampling::nearest))
    {
        throw std::invalid_argument("Invalid image presentation options");
    }
}

} // namespace

image_widget::image_widget(bitmap value, image_options options, centui::style_source appearance)
    : image_widget(image_source::from_bitmap(std::move(value)), options, std::move(appearance))
{
}

image_widget::image_widget(image_source value, image_options options, centui::style_source appearance)
    : widget(std::move(appearance)),
      m_image(std::move(value)),
      m_options(options)
{
    (void)m_image.intrinsic();
    validate(options);
}

void image_widget::set_image(bitmap value)
{
    check_mutation();
    set_image(image_source::from_bitmap(std::move(value)));
}

void image_widget::set_image(image_source value)
{
    check_mutation();
    const auto intrinsic = value.intrinsic();
    if(value.identity() == m_image.identity())
    {
        return;
    }
    const auto layout_changed = intrinsic != m_image.intrinsic();
    m_image = std::move(value);
    if(layout_changed)
    {
        invalidate_layout();
    }
    else
    {
        invalidate_paint();
    }
}

const image_source& image_widget::value() const noexcept
{
    return m_image;
}

void image_widget::set_options(image_options value)
{
    check_mutation();
    validate(value);
    if(value != m_options)
    {
        m_options = value;
        invalidate_paint();
    }
}

image_options image_widget::options() const noexcept
{
    return m_options;
}

size image_widget::measure(const measure_context& context)
{
    const auto intrinsic = m_image.intrinsic();
    const auto explicit_ratio = resolved_style().aspect_ratio;
    const auto ratio = explicit_ratio.value_or(intrinsic.aspect_ratio.value_or(0));
    const bool exact_width = context.width.mode == constraint_mode::exact;
    const bool exact_height = context.height.mode == constraint_mode::exact;
    float width = intrinsic.width.value_or(300);
    float height = intrinsic.height.value_or(150);

    if(!intrinsic.width && context.width.mode != constraint_mode::unconstrained)
    {
        width = context.width.value;
    }
    if(!intrinsic.height && context.height.mode != constraint_mode::unconstrained)
    {
        height = context.height.value;
    }
    if(exact_width)
    {
        width = context.width.value;
    }
    if(exact_height)
    {
        height = context.height.value;
    }

    if(ratio > 0)
    {
        if(exact_width && !exact_height)
        {
            height = width / ratio;
        }
        else if(exact_height && !exact_width)
        {
            width = height * ratio;
        }
        else if(!exact_width && !exact_height)
        {
            if((intrinsic.width && !intrinsic.height) || explicit_ratio)
            {
                height = width / ratio;
            }
            else if(intrinsic.height && !intrinsic.width)
            {
                width = height * ratio;
            }
            else if(!intrinsic.width && !intrinsic.height)
            {
                if(width / ratio <= height)
                {
                    height = width / ratio;
                }
                else
                {
                    width = height * ratio;
                }
            }

            float scale = 1;
            if(context.width.mode == constraint_mode::maximum && width > 0)
            {
                scale = std::min(scale, context.width.value / width);
            }
            if(context.height.mode == constraint_mode::maximum && height > 0)
            {
                scale = std::min(scale, context.height.value / height);
            }
            width *= scale;
            height *= scale;
        }
    }

    if(context.width.mode == constraint_mode::maximum)
    {
        width = std::min(width, context.width.value);
    }
    if(context.height.mode == constraint_mode::maximum)
    {
        height = std::min(height, context.height.value);
    }
    return {std::max(0.0f, width), std::max(0.0f, height)};
}

void image_widget::paint(painter& output) const
{
    const auto area = output.bounds();
    if(area.width <= 0 || area.height <= 0)
    {
        return;
    }
    const auto intrinsic = m_image.intrinsic();
    const auto ratio =
        intrinsic.aspect_ratio.value_or(intrinsic.width.value_or(area.width) / intrinsic.height.value_or(area.height));
    float width = area.width;
    float height = area.height;
    if(m_options.fit != image_fit::fill)
    {
        const bool use_width =
            m_options.fit == image_fit::contain ? area.width / area.height <= ratio : area.width / area.height >= ratio;
        if(use_width)
        {
            height = width / ratio;
        }
        else
        {
            width = height * ratio;
        }
    }
    auto prepared = m_image.prepare({width, height}, output.scale());
    output.image(
        std::move(prepared),
        {area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height},
        {255, 255, 255, 255},
        m_options.sampling
    );
}

} // namespace centui
