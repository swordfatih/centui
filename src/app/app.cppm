module;

#include <string>

export module centui.app;

import centui.window;
import centui.bgfx;
export import centui.tree;

export namespace centui
{

struct app_options
{
    std::string title;
    int         width;
    int         height;
};

class app
{
public:
    explicit app(const app_options& options)
        : _window(options.title, options.width, options.height),
          _backend(_window.viewport()), _ui(_backend)
    {
    }

    app(const app&) = delete;
    app& operator=(const app&) = delete;
    app(app&&) = delete;
    app& operator=(app&&) = delete;

    [[nodiscard]] bool poll_events()
    {
        return _window.poll_events();
    }

    [[nodiscard]] bool poll_events(document& owner)
    {
        owner.set_host_services(_window.services());
        if(!_window.poll_events())
        {
            return false;
        }
        const auto events = _window.take_input_events();
        _ui.process_events(owner, events);
        return true;
    }

    void render_frame(document& owner)
    {
        if(_window.begin_frame())
        {
            _ui.render(owner);
            _window.end_frame();
        }
    }

    void run(document& owner)
    {
        while(poll_events(owner))
        {
            render_frame(owner);
        }
    }

private:
    // Reverse destruction releases GPU resources before the graphics context.
    window        _window;
    bgfx::backend _backend;
    ui            _ui;
};

} // namespace centui
