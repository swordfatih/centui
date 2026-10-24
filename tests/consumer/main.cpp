#include <centui/centui.hpp>

int main()
{
    auto root = centui::container{};
    root.patch_style(
        {.width = centui::px(100),
         .height = centui::px(100),
         .background_color = centui::states_color::all({255, 0, 0, 255})}
    );
    centui::document document{root};
    return document.prepare({100, 100}).commands.empty() ? 1 : 0;
}
