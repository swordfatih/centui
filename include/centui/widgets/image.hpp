#pragma once

#include <algorithm>
#include <memory>
#include <utility>

#include <centui/tree.hpp>

namespace centui
{

/// \brief Draws a shared bitmap while preserving its aspect ratio.
class image_widget : public widget
{
public:
    explicit image_widget(bitmap value, centui::style appearance = {});

    void set_image(bitmap value);

protected:
    size measure(const measure_context&) override;

    void paint(painter& output) const override;

private:
    std::shared_ptr<const bitmap> m_image;
};

} // namespace centui
