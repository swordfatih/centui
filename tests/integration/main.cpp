#include <centui/centui.hpp>

#include CENTUI_PLATFORM_HEADER
#include CENTUI_BACKEND_HEADER

namespace
{

template <typename Platform, typename Renderer>
int run_integration()
{
    Platform host{{.title = "CentUI integration test", .width = 160, .height = 120}};
    Renderer renderer{host};
    centui::ui integration{host, renderer};

    auto root = centui::container{centui::text{"Integration test"}, centui::button{"Button"}};
    root.patch_style({.width = centui::percent(100), .height = centui::percent(100)});

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});

    int frames = 0;

    for(int i = 0; i < 4; ++i)
    {
        if(!host.poll_events())
        {
            return 1;
        }

        if(integration.frame(document))
        {
            ++frames;
        }
    }

    return frames == 4 ? 0 : 3;
}

} // namespace

int main()
{
    return run_integration<CENTUI_PLATFORM_TYPE, CENTUI_BACKEND_TYPE>();
}
