#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <memory>

import centui.tree;
import centui.widgets;

class meter final : public centui::widget
{
public:
    explicit meter(float value = 0) : _value(std::clamp(value, 0.0f, 1.0f))
    {
    }

    void set_value(float value)
    {
        check_mutation();
        value = std::clamp(value, 0.0f, 1.0f);
        if(value != _value)
        {
            _value = value;
            invalidate_paint();
        }
    }

protected:
    centui::size measure(const centui::measure_context&) override
    {
        return {120, 24};
    }

    void paint(centui::painter& output) const override
    {
        auto fill = output.bounds();
        output.rectangle(fill, centui::color::from_hex(0x334155));
        fill.width *= _value;
        output.rectangle(fill, centui::color::from_hex(0x38BDF8));
    }

private:
    float _value;
};

int main()
{
    auto progress = centui::make_widget<meter>(0.25f);
    centui::container<> root{progress};
    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    progress->set_value(0.75f);
    const auto& list = document.prepare({320, 200});
    return list.commands.empty() ? 1 : 0;
}
