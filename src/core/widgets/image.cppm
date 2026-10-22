module;

#include <algorithm>
#include <memory>
#include <utility>

export module centui.widgets.image;
export import centui.tree;

export namespace centui
{
class image_widget : public widget
{
public:
    explicit image_widget(bitmap value, centui::style appearance = {}) : widget(std::move(appearance))
    {
        set_image(std::move(value));
    }

    void set_image(bitmap value)
    {
        check_mutation();
        validate_bitmap(value);
        _image = std::make_shared<const bitmap>(std::move(value));
        invalidate_layout();
    }

protected:
    size measure(const measure_context&) override
    {
        return {static_cast<float>(_image->width), static_cast<float>(_image->height)};
    }

    void paint(painter& output) const override
    {
        const auto area = output.bounds();
        const auto scale = std::min(area.width / _image->width, area.height / _image->height);
        const auto width = _image->width * scale;
        const auto height = _image->height * scale;
        output.image(_image, {area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height});
    }

private:
    std::shared_ptr<const bitmap> _image;
};
}
