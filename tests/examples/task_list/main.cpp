#include <string>

#include <centui/backends/bgfx.hpp>
#include <centui/centui.hpp>
#include <centui/platforms/sdl.hpp>

namespace
{

struct page
{
    centui::length width = centui::percent(100);
    centui::length height = centui::percent(100);
    centui::edges padding = centui::edges::all(centui::px(24));
    centui::length gap = centui::px(16);
    float font_size = 18;
    centui::states_color text_color = centui::states_color::all({226, 232, 240, 255});
    centui::states_color background_color = centui::states_color::all({15, 23, 42, 255});
};

struct row
{
    centui::direction flex_direction = centui::direction::row;
    centui::alignment align_items = centui::alignment::center;
    centui::length gap = centui::px(12);
};

struct fill
{
    centui::flex flex = centui::flex::all(1, 1, centui::px(0));
};

struct control
{
    centui::edges padding = centui::edges::all(centui::px(12));
    float corner_radius = 6;
    centui::states_color background_color{
        .normal = {30, 41, 59, 255},
        .hover = {51, 65, 85, 255},
        .pressed = {71, 85, 105, 255},
    };
};

} // namespace

int main()
{
    centui::sdl::platform window{{.title = "CentUI / Task list", .width = 720, .height = 520}};
    centui::bgfx::backend renderer{window};

    auto entry = centui::input<control, fill>{};
    auto tasks = centui::container{};
    tasks->patch_style({.gap = centui::px(12)});

    // Both controls use the same action; the tree is updated without rebuilding the document.
    const auto add_task = [&entry, &tasks]
    {
        const std::string title{entry->value()};

        if(title.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            return;
        }

        auto complete = centui::toggle<fill>{title};
        auto remove = centui::button<control>{"Remove"};
        auto task = centui::container<row>{complete, remove};

        complete->set_on_change(
            [weak = task.weak()](bool checked)
            {
                if(auto item = weak.lock(); item.valid())
                {
                    item->patch_style(
                        {.text_color = centui::states_color::all(
                             checked ? centui::color{100, 116, 139, 255} : centui::color{226, 232, 240, 255}
                         )}
                    );
                }
            }
        );

        // The button observes its ancestors so its callback cannot keep a removed row alive.
        remove->set_on_click(
            [list = tasks.weak(), weak = task.weak()]
            {
                auto parent = list.lock();
                auto item = weak.lock();

                if(parent.valid() && item.valid())
                {
                    parent->remove(item);
                }
            }
        );

        tasks->append(task);
        entry->set_text("");
    };

    entry->set_on_submit(add_task);

    auto list = centui::scroll<fill>{tasks};

    auto root = centui::container<page>{
        centui::text{"Things to do"},
        centui::text{"Type a task, then press Enter or click Add."},
        centui::container<row>{
            entry,
            centui::button<control>{
                "Add",
                {
                    .on_click = add_task,
                }
            },
        },
        list,
    };

    centui::document document{root};

    centui::ui ui{window, renderer};

    while(window.poll_events())
    {
        ui.frame(document);
    }
}
