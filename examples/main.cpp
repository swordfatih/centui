
import std;

import centui.app;
import centui.container;
import centui.text;
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
    centui::app app{{.title = "CentUI", .width = 800, .height = 600}};

    auto root = centui::container<panel_style>{
        centui::text<title_style>{"Hello, CentUI!"},
        centui::container<child_style>{
            centui::text<title_style>{"This is a nested text..."},
        },
    };

    app.run(root);
}
