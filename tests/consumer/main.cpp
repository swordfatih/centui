#include <centui/centui.hpp>

int main()
{
    auto root = centui::container{centui::text{"System fonts\r\nand explicit registration"}};
    root->patch_style({
        .width = centui::px(100),
        .height = centui::px(100),
        .background_color = centui::states_color::all({255, 0, 0, 255}),
    });
    centui::document document{root};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    return document.prepare({100, 100}).commands.empty() ? 1 : 0;
}
