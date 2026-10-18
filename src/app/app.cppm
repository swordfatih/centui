export module centui.app;

import std;
import centui.window;
import centui.bgfx;
import centui.ui;
export import centui.element;

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

    [[nodiscard]] bool poll_events(element& root)
    {
        if(!_window.poll_events())
        {
            return false;
        }

        const auto events = _window.take_input_events();
        _ui.process_events(root, events);
        return true;
    }

    void render_frame(element& root)
    {
        if(_window.begin_frame())
        {
            _ui.render(root);
            _window.end_frame();
        }
    }

    void run(element& root)
    {
        while(poll_events(root))
        {
            render_frame(root);
        }
    }

private:
    // Reverse destruction: UI, GPU resources/font, then bgfx and SDL window.
    window _window;
    bgfx::backend _backend;
    ui<bgfx::backend> _ui;
};

} // namespace centui
