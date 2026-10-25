#include <memory>
#include <string>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

#include <emscripten/html5.h>

namespace
{

struct screen
{
    centui::length width = centui::percent(100);
    centui::length height = centui::percent(100);
    centui::alignment align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    centui::states_color background_color = centui::states_color::all({15, 23, 42, 255});
};

struct card
{
    centui::length width = centui::percent(90);
    centui::length max_width = centui::px(380);
    centui::edges padding = centui::edges::all(centui::px(24));
    centui::length gap = centui::px(16);
    float corner_radius = 16;
    centui::states_color background_color = centui::states_color::all({30, 41, 59, 255});
};

struct control
{
    centui::length height = centui::px(44);
    centui::edges padding = centui::edges::all(centui::px(12));
    centui::states_color background_color = centui::states_color::all({37, 99, 235, 255});
};

auto make_interface()
{
    auto clicks = std::make_shared<int>(0);
    auto label = centui::text{"0 clicks"};
    const auto increment = [clicks, label]() mutable
    {
        label.set_text(std::to_string(++*clicks) + " clicks");
    };

    return centui::container<screen>{
        centui::container<card>{
            centui::text{"Hello from CentUI on the web"},
            centui::text{"Resize the browser, click the button, or type below."},
            label,
            centui::button<control>{"Click me", {.on_click = increment}},
            centui::input<control>{"Type here"},
        },
    };
}

} // namespace

int main()
{
    // Browser callbacks continue after main returns; the example owns this lifetime.
    static centui::sdl::platform platform{{.title = "CentUI web", .width = 900, .height = 640}};
    static centui::bgfx::backend renderer{platform};
    static centui::document document{make_interface()};
    static centui::ui ui{platform, renderer};

    document.set_font(centui::font{CENTUI_FONT_PATH});

    emscripten_request_animation_frame_loop(
        [](double, void*) -> EM_BOOL
        {
            if(!platform.poll_events())
            {
                return EM_FALSE;
            }

            ui.frame(document);

            return EM_TRUE;
        },
        nullptr
    );

    return 0;
}
