#include <cstddef>
#include <string>
#include <vector>

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

struct task_row : row
{
    centui::length height = centui::percent(100);
    centui::edges padding{
        .bottom = centui::px(12),
    };
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

struct task
{
    std::string title;
    bool completed = false;
};

centui::states_color task_color(bool completed)
{
    return centui::states_color::all(completed ? centui::color{100, 116, 139, 255} : centui::color{226, 232, 240, 255});
}

} // namespace

int main()
{
    centui::sdl::platform window{{.title = "CentUI / Task list", .width = 720, .height = 520}};
    centui::bgfx::backend renderer{window};

    // Task values outlive rows, which are created and removed as the list scrolls.
    std::vector<task> items;
    centui::handle<centui::list_widget> tasks;

    tasks = centui::list<fill>{
        [&items, &tasks](std::size_t index)
        {
            const auto& item = items[index];

            auto complete = centui::toggle<fill>{
                item.title,
                {
                    .checked = item.completed,
                }
            };
            auto remove = centui::button<control>{"Remove"};
            auto row = centui::container<task_row>{complete, remove};
            row->patch_style({
                .text_color = task_color(item.completed),
            });

            complete->set_on_change(
                [&items, index, weak = row.weak()](bool checked)
                {
                    items[index].completed = checked;

                    if(auto current = weak.lock(); current.valid())
                    {
                        current->patch_style({
                            .text_color = task_color(checked),
                        });
                    }
                }
            );

            remove->set_on_click(
                [&items, index, weak = tasks.weak()]
                {
                    if(auto list = weak.lock(); list.valid())
                    {
                        items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));

                        list->set_item_count(items.size());
                        // Erasing shifts indexes; rebuild mounted rows with their new indexes.
                        list->refresh();
                    }
                }
            );

            return row;
        },
        {
            .item_count = items.size(),
            .row_height = 64,
        }
    };

    auto entry = centui::input<control, fill>{};

    const auto add_task = [&items, &entry, &tasks]
    {
        const std::string title{entry->value()};

        if(title.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            return;
        }

        items.push_back({
            .title = title,
        });

        tasks->set_item_count(items.size());
        tasks->scroll_to(items.size() - 1);
        entry->set_text("");
    };

    entry->set_on_submit(add_task);

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
        tasks,
    };

    centui::document document{root};

    centui::ui ui{window, renderer};

    while(window.poll_events())
    {
        ui.frame(document);
    }
}
