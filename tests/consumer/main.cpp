#include <centui/centui.hpp>

#if defined(__EMSCRIPTEN_PTHREADS__)
#error "The CentUI web consumer must remain single threaded"
#endif

int main()
{
    auto root = centui::container{centui::text{"System fonts\r\nand explicit registration"}};
    root.patch_style({
        .width = centui::px(100),
        .height = centui::px(100),
        .background_color = centui::states_color::all({255, 0, 0, 255}),
    });
    centui::document document{root};
#if defined(__EMSCRIPTEN__)
    document.register_font(std::filesystem::path{"/assets/fonts/Inter-Regular.ttf"});
#endif
    return document.prepare({100, 100}).commands.empty() ? 1 : 0;
}
