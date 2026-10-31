#pragma once

#include <algorithm>
#include <memory>
#include <utility>

#include <centui/image.hpp>
#include <centui/widget.hpp>

namespace centui
{

/// \brief Draws a shared bitmap while preserving its aspect ratio.
class image_widget : public widget
{
public:
    explicit image_widget(bitmap value, image_options options = {}, centui::style_source appearance = {});

    explicit image_widget(image_source value, image_options options = {}, centui::style_source appearance = {});

    void set_image(bitmap value);

    void set_image(image_source value);

    [[nodiscard]] const image_source& value() const noexcept;

    void set_options(image_options value);

    [[nodiscard]] image_options options() const noexcept;

protected:
    size measure(const measure_context&) override;

    void paint(painter& output) const override;

private:
    image_source m_image;
    image_options m_options;
};

template <typename... Styles>
class image : public handle<image_widget>
{
public:
    explicit image(image_source value, image_options options = {}, centui::style_source appearance = {})
        : handle<image_widget>(make_widget<image_widget>(
              std::move(value),
              options,
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          ))
    {
    }

    explicit image(bitmap value, image_options options = {}, centui::style_source appearance = {})
        : image(
              image_source::from_bitmap(std::move(value)),
              options,
              centui::style_source::with_composition<Styles...>(std::move(appearance))
          )
    {
    }
};

} // namespace centui
