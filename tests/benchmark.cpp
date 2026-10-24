#include <chrono>
#include <iostream>

#include <centui/centui.hpp>

class tile : public centui::widget
{
public:
    tile()
    {
        patch_style({.width = centui::px(10), .height = centui::px(10)});
    }

    void change()
    {
        invalidate_paint();
    }

    void paint(centui::painter& painter) const override
    {
        painter.rectangle(painter.bounds(), {255, 0, 0, 255});
    }
};

int main()
{
    for(bool cached: {false, true})
    {
        auto root = centui::container{};
        root.patch_style({.width = centui::px(1000), .height = centui::px(1000)});
        auto panel = centui::container{};
        panel.patch_style({.height = centui::px(900)});
        panel.patch_style({.flex_direction = centui::direction::row});
        for(int i = 0; i < 100; ++i)
        {
            panel->append(centui::make_widget<tile>());
        }
        panel->set_paint_boundary(cached);
        auto animated = centui::make_widget<tile>();
        root->append(panel);
        root->append(animated);
        centui::document document{root};
        (void)document.prepare({1000, 1000});
        const auto start = std::chrono::steady_clock::now();
        for(int frame = 0; frame < 1000; ++frame)
        {
            animated->change();
            (void)document.prepare({1000, 1000});
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
        std::cout << "cached=" << cached << " ms=" << elapsed.count()
                  << " paint_visits=" << document.statistics().paint_visits
                  << " cache_bytes=" << document.statistics().command_cache_bytes << '\n';
    }
}
