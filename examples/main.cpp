
import std;

import centui.window;
import centui.renderer;
import centui.container;
import centui.text;
import centui.font;
import centui.color;
import centui.unit;

struct panel_style
{
    centui::length width = centui::px(400);
    centui::length height = centui::px(300);
    centui::edges  padding = centui::edges::all(centui::px(20));
    centui::color  background_color = centui::color::from_hex(0x334155);
};

struct child_style
{
    centui::length width = centui::fit();
    centui::length height = centui::px(100);
    centui::color  background_color = centui::color::from_hex(0x38BDF8);
};

struct title_style
{
    float         font_size = 32.0f;
    centui::color text_color = centui::color::from_hex(0xFFFFFF);
};

int main()
{
    centui::window   window{"CentUI", 800, 600};
    centui::font     face;
    centui::renderer renderer{face};

    centui::container<panel_style> root{
        centui::text<title_style>{"Hello, CentUI!"},
        centui::container<child_style>{
            centui::text<title_style>{"This is a nested text..."},
        },
    };

    root.node.attach(&face);

    while(window.poll_events())
    {
        if(!window.begin_frame())
        {
            continue;
        }

        root.node.calculate(window.width(), window.height());

        renderer.begin(window.width(), window.height());
        renderer.draw(root.node);

        window.end_frame();
    }
}
