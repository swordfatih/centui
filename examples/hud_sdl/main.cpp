#include <format>
#include <initializer_list>
#include <memory>
#include <stdexcept>

#include <SDL3/SDL.h>

import centui;
import centui.sdl_renderer;

namespace
{
struct screen
{
    centui::length        width = centui::percent(100);
    centui::length        height = centui::percent(100);
    centui::alignment     align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
};

struct card
{
    centui::length       width = centui::px(320);
    centui::edges        padding = centui::edges::all(centui::px(24));
    centui::length       gap = centui::px(16);
    centui::states_color background_color = centui::states_color::all({30, 41, 59, 240});
};

struct title
{
    float font_size = 28;
};

struct action
{
    centui::length height = centui::px(44);
    centui::edges  padding = centui::edges::all(centui::px(12));
    struct
    {
        centui::color normal{37, 99, 235, 255};
        centui::color hover{59, 130, 246, 255};
        centui::color pressed{29, 78, 216, 255};
    } background_color;
};
} // namespace

int main()
{
    centui::sdl::platform platform{{.title = "CentUI + SDL renderer", .width = 720, .height = 480}};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> native_renderer{
        SDL_CreateRenderer(&platform.native_window(), nullptr), SDL_DestroyRenderer};
    if(!native_renderer)
    {
        throw std::runtime_error(SDL_GetError());
    }

    centui::sdl::backend renderer{*native_renderer, platform};
    centui::ui ui{platform, renderer};
    auto count = std::make_shared<int>(0);
    auto label = centui::text{"0 clicks"};

    // clang-format off
    auto root = centui::container<screen>{
        centui::container<card>{
            centui::text<title>{"Hello, SDL."},
            centui::text{"The blue strip is drawn by your SDL renderer."},
            label,
            centui::button<action>{"Count a click", {
                .on_click = [count, label]() mutable
                {
                    label.set_text(std::format("{} clicks", ++*count));
                },
            }},
        },
    };
    // clang-format on

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});

    while(platform.poll_events())
    {
        ui.process_events(document, platform.events());
        if(!renderer.begin_frame({15, 23, 42, 255}))
        {
            continue;
        }

        if(!SDL_SetRenderDrawColor(native_renderer.get(), 37, 99, 235, 255))
        {
            throw std::runtime_error(SDL_GetError());
        }
        const SDL_FRect strip{0, 0, static_cast<float>(platform.viewport().width), 90};
        if(!SDL_RenderFillRect(native_renderer.get(), &strip))
        {
            throw std::runtime_error(SDL_GetError());
        }

        ui.render(document);
        renderer.end_frame();
    }
}
