#include <centui/centui.hpp>
#include <centui/texture_cache.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class probe : public centui::widget
{
public:
    mutable int paints = 0;

    probe()
    {
        patch_style({.width = centui::px(20), .height = centui::px(20)});
    }

    void change()
    {
        invalidate_paint();
    }

    void paint(centui::painter& painter) const override
    {
        ++paints;
        painter.rectangle(painter.bounds(), {255, 0, 0, 255});
    }
};

} // namespace

TEST_CASE("Paint boundaries reuse unchanged siblings and invalidate on geometry", "[cache]")
{
    auto a = centui::make_widget<probe>();
    auto b = centui::make_widget<probe>();
    a->set_paint_boundary(true);
    auto root = centui::container{a, b};
    centui::document document{root};
    (void)document.prepare({200, 100});
    b->change();
    (void)document.prepare({200, 100});
    REQUIRE(a->paints == 1);
    REQUIRE(b->paints == 2);
    REQUIRE(document.statistics().command_cache_hits == 1);
    a->change();
    (void)document.prepare({200, 100});
    REQUIRE(a->paints == 2);
    root->set_enabled(false);
    (void)document.prepare({200, 100});
    REQUIRE(a->paints == 3);
    (void)document.prepare({300, 100});
    REQUIRE(a->paints == 4);
    document.set_command_cache_budget(0);
    b->change();
    (void)document.prepare({300, 100});
    REQUIRE(a->paints == 5);
    REQUIRE(document.statistics().command_cache_bytes == 0);
}

TEST_CASE("Topology edits retain unaffected layout nodes", "[layout][cache]")
{
    auto a = centui::make_widget<probe>();
    auto root = centui::container{a};
    centui::document document{root};
    (void)document.prepare({200, 100});
    const auto before = document.statistics().layout_nodes_created;
    auto b = centui::make_widget<probe>();
    document.append(root, b);
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().layout_nodes_created == before + 1);
    document.remove(b);
    (void)document.prepare({200, 100});
    REQUIRE(document.statistics().layout_nodes_created == before + 1);
}

TEST_CASE("Texture eviction preserves hot and current-frame resources", "[cache]")
{
    centui::details::texture_cache<int> cache;
    auto a = std::make_shared<centui::bitmap>(centui::bitmap{1, 1, {255}, centui::pixel_format::alpha});
    auto b = std::make_shared<centui::bitmap>(*a);
    auto c = std::make_shared<centui::bitmap>(*a);
    cache.emplace(a, 1);
    cache.emplace(b, 2);
    REQUIRE(cache.find(a) != cache.end());
    centui::draw_list list{{100, 100}, {{{0, 0, 1, 1}, {}, c}}};
    cache.begin_frame(list, 2, 8);
    REQUIRE(cache.find(a) != cache.end());
    REQUIRE(cache.find(b) == cache.end());
    cache.emplace(c, 3);
    REQUIRE(cache.statistics().bytes == 8);
    list.commands.push_back({{0, 0, 1, 1}, {}, a});
    cache.begin_frame(list, 1, 0);
    REQUIRE(cache.find(a) != cache.end());
    REQUIRE(cache.find(c) != cache.end());
    cache.begin_frame({}, 1, 0);
    REQUIRE(cache.statistics().bytes == 0);
}
