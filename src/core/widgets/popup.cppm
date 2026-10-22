module;

#include <functional>
#include <utility>

export module centui.widgets.popup;
export import centui.widgets.container;
import centui.widgets.callback;

export namespace centui
{
class popup_widget : public container_widget
{
public:
    explicit popup_widget(handle<> content, centui::style appearance = {}) : container_widget(std::move(appearance))
    {
        patch_style({.inset = edges::all(px(0)), .position = position::absolute,
                     .width = percent(100), .height = percent(100), .visibility = visibility::collapsed});
        append(content);
        set_interactive();
        set_focusable();
        set_focus_scope();
    }

    void open()
    {
        check_mutation();
        if(style().visibility == visibility::visible)
        {
            return;
        }

        auto root = parent();
        while(root.valid() && root->parent().valid())
        {
            root = root->parent();
        }

        if(root.valid())
        {
            _previous_focus = find_focus(root).weak();
        }

        _focus_pending = true;
        patch_style({.visibility = visibility::visible});
    }

    void close()
    {
        check_mutation();
        if(style().visibility != visibility::visible)
        {
            return;
        }

        _focus_pending = false;
        patch_style({.visibility = visibility::collapsed});
        if(auto previous = _previous_focus.lock(); previous.valid())
        {
            previous->request_focus();
        }

        _on_close();
    }

    void set_on_close(std::function<void()> callback)
    {
        check_mutation();
        _on_close.set(std::move(callback));
    }

protected:
    void on_layout() override
    {
        if(_focus_pending && effectively_visible())
        {
            _focus_pending = false;
            focus_next();
        }
    }

    bool on_event(const input_event& event) override
    {
        if(event.type == input_event_type::key_pressed && event.code == key::escape)
        {
            close();
            return true;
        }

        if(event.type == input_event_type::pointer_pressed && event.button == mouse_button::primary)
        {
            for(const auto& child: children())
            {
                const auto box = child->bounds();
                if(event.x >= box.x && event.y >= box.y && event.x < box.x + box.width && event.y < box.y + box.height)
                {
                    return true;
                }
            }

            close();
            return true;
        }

        return event.type == input_event_type::wheel;
    }

private:
    static handle<> find_focus(const handle<>& root)
    {
        if(root->focused())
        {
            return root;
        }

        for(const auto& child: root->children())
        {
            if(auto result = find_focus(child); result.valid())
            {
                return result;
            }
        }

        return {};
    }

    bool _focus_pending = false;
    weak_handle<> _previous_focus;
    details::callback<> _on_close;
};

template <typename... Styles>
class popup : public handle<popup_widget>
{
public:
    explicit popup(handle<> content, centui::style appearance = centui::style::resolve<Styles...>())
        : handle<popup_widget>(make_widget<popup_widget>(std::move(content), std::move(appearance)))
    {
    }
};
}
