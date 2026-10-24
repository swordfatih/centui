#include <centui/centui.hpp>
#if defined(CENTUI_SMOKE_SDL) || defined(CENTUI_SMOKE_SDL_BGFX)
#include <centui/platforms/sdl.hpp>
using host_type = centui::sdl::platform;
#elif defined(CENTUI_SMOKE_SFML)
#include <centui/platforms/sfml.hpp>
using host_type = centui::sfml::platform;
#else
#include <centui/platforms/glfw.hpp>
using host_type = centui::glfw::platform;
#endif
#if defined(CENTUI_SMOKE_SDL)
#include <centui/backends/sdl.hpp>
using renderer_type = centui::sdl::backend;
#elif defined(CENTUI_SMOKE_SFML)
#include <centui/backends/sfml.hpp>
using renderer_type = centui::sfml::backend;
#else
#include <centui/backends/bgfx.hpp>
using renderer_type = centui::bgfx::backend;
#endif

int main()
{
    host_type host{{.title = "CentUI smoke", .width = 160, .height = 120}};
    renderer_type renderer{host};
    auto root = centui::container{centui::text{"Smoke"}, centui::button{"Button"}};
    root.patch_style({.width = centui::percent(100), .height = centui::percent(100)});
    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::ui integration{host, renderer};
    int frames = 0;
    for(int i = 0; i < 4; ++i)
    {
        if(!host.poll_events())
        {
            return 1;
        }
        integration.process_events(document, host.events());
        if(renderer.begin_frame())
        {
            integration.render(document);
            renderer.end_frame();
            ++frames;
        }
        if(i == 1)
        {
#if defined(CENTUI_SMOKE_SDL) || defined(CENTUI_SMOKE_SDL_BGFX)
            if(!SDL_SetWindowSize(&host.native_window(), 200, 140))
            {
                return 2;
            }
#elif defined(CENTUI_SMOKE_SFML)
            host.native_window().setSize({200, 140});
#else
            glfwSetWindowSize(&host.native_window(), 200, 140);
#endif
        }
    }
    return frames == 4 ? 0 : 3;
}
