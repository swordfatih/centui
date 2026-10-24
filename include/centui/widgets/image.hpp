#pragma once

#include <algorithm>
#include <memory>
#include <utility>

#include <centui/image.hpp>
#include <centui/tree.hpp>

namespace centui
{

/// \brief Draws a shared bitmap while preserving its aspect ratio.
class image_widget : public widget
{
public:
    explicit image_widget(bitmap value, centui::style appearance = {});

    explicit image_widget(image_source value, image_options options = {}, centui::style appearance = {});

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
    explicit image(
        image_source value,
        image_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : handle<image_widget>(make_widget<image_widget>(std::move(value), options, std::move(appearance)))
    {
    }

    explicit image(
        bitmap value,
        image_options options = {},
        centui::style appearance = centui::style::resolve<Styles...>()
    )
        : image(image_source::from_bitmap(std::move(value)), options, std::move(appearance))
    {
    }

    void set_image(image_source value)
    {
        this->get().set_image(std::move(value));
    }

    void set_image(bitmap value)
    {
        this->get().set_image(std::move(value));
    }

    [[nodiscard]] const image_source& value() const
    {
        return this->get().value();
    }

    void value(image_source value)
    {
        set_image(std::move(value));
    }

    void set_options(image_options value)
    {
        this->get().set_options(value);
    }

    [[nodiscard]] image_options options() const
    {
        return this->get().options();
    }
};

} // namespace centui
