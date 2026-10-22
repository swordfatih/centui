module;

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <memory>

export module centui.widgets.toggle;
export import centui.widgets.button;
import centui.widgets.callback;

export namespace centui
{
class toggle_widget : public button_widget
{
public:
    explicit toggle_widget(std::string label, centui::style appearance = {}, bool checked = false)
        : button_widget(std::move(label), std::move(appearance)), _checked(checked)
    {
    }

    [[nodiscard]] bool checked() const noexcept
    {
        return _checked;
    }

    void set_checked(bool value)
    {
        check_mutation();
        if(_checked != value)
        {
            _checked = value;
            invalidate_paint();
        }
    }

    void set_on_change(std::function<void(bool)> callback)
    {
        check_mutation();
        _on_change.set(std::move(callback));
    }

protected:
    void activate() override
    {
        set_checked(!_checked);
        _on_change(_checked);
        button_widget::activate();
    }

    void paint(painter& output) const override
    {
        if(_checked)
        {
            auto line = output.bounds();
            line.y += std::max(0.0f, line.height - 2);
            line.height = std::min(2.0f, line.height);
            output.rectangle(line, output.foreground());
        }
    }

private:
    bool _checked = false;
    details::callback<bool> _on_change;
};

struct toggle_options
{
    bool checked = false;
    std::function<void(bool)> on_change{};
};

template <typename... Styles>
class toggle : public handle<toggle_widget>
{
public:
    explicit toggle(std::string label, toggle_options options = {},
                    centui::style appearance = centui::style::resolve<Styles...>())
        : handle<toggle_widget>(make_widget<toggle_widget>(std::move(label), std::move(appearance), options.checked))
    {
        this->get().set_on_change(std::move(options.on_change));
    }
};
}
