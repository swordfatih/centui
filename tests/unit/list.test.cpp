#include <limits>
#include <map>

#include <centui/document.hpp>
#include <centui/widgets/list.hpp>
#include <centui/widgets/pressable.hpp>
#include <centui/widgets/toggle.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

void size_list(const centui::handle<centui::list_widget>& control, float height = 100)
{
    control->patch_style({.width = centui::px(200), .height = centui::px(height)});
}

} // namespace

TEST_CASE("Large lists mount only the viewport and overscan and retain overlapping rows", "[widget][list]")
{
    std::size_t created = 0;
    std::map<std::size_t, centui::weak_handle<>> rows;

    auto control = centui::list{
        [&](std::size_t index)
        {
            ++created;

            auto row = centui::container{};
            rows[index] = centui::handle<>{row}.weak();

            return row;
        },
        {
            .item_count = 100000,
            .row_height = 20,
            .overscan = 2,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    REQUIRE(created == 0);

    (void)document.prepare({200, 100});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 7});
    REQUIRE(created == 7);
    REQUIRE(control->limit().y == Catch::Approx(1999900));
    REQUIRE(document.statistics().layout_nodes_created < 30);

    auto retained = rows[4].lock();

    control->set_offset({0, 100});
    (void)document.prepare({200, 100});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{3, 12});
    REQUIRE(created == 12);
    REQUIRE(rows[0].lock().valid() == false);
    REQUIRE(rows[4].lock() == retained);
    REQUIRE(retained->bounds().y == Catch::Approx(-20));

    const auto before = created;
    (void)document.prepare({200, 100});

    REQUIRE(created == before);
    REQUIRE(document.pending_changes().any() == false);
}

TEST_CASE("Lists reveal requested items before the first frame and handle shrinking and empty data", "[widget][list]")
{
    auto control = centui::list{
        [](std::size_t)
        {
            return centui::container{};
        },
        {
            .item_count = 100000,
            .row_height = 20,
            .overscan = 1,
        }
    };

    size_list(control);
    control->scroll_to(99999);

    centui::document document{control, {.system_font_discovery = false}};
    (void)document.prepare({200, 100});

    REQUIRE(control->offset().y == control->limit().y);
    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{99994, 100000});

    control->set_item_count(3);
    (void)document.prepare({200, 100});

    REQUIRE(control->item_count() == 3);
    REQUIRE(control->offset().y == 0);
    REQUIRE(control->limit().y == 0);
    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 3});

    control->set_item_count(0);
    (void)document.prepare({200, 100});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 0});
    REQUIRE(control->children().front()->children().empty());
    REQUIRE(control->limit().y == 0);
    REQUIRE_THROWS_AS(control->scroll_to(0), std::out_of_range);

    control->set_item_count(100);
    control->scroll_to(50);
    (void)document.prepare({200, 100});

    const auto [first, end] = control->rendered_range();

    REQUIRE(first <= 50);
    REQUIRE(end > 50);
}

TEST_CASE("List ranges follow fractional scrolling, viewport resizing and DPI", "[widget][list][dpi]")
{
    auto control = centui::list{
        [](std::size_t)
        {
            return centui::container{};
        },
        {
            .item_count = 100,
            .row_height = 20,
            .overscan = 0,
        }
    };

    control->patch_style({.width = centui::percent(100), .height = centui::percent(100)});

    centui::document document{control, {.system_font_discovery = false}};
    (void)document.prepare({400, 200, 2});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 5});

    control->set_offset({0, 0.5f});
    (void)document.prepare({400, 200, 2});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 6});

    (void)document.prepare({400, 400, 2});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 11});
    REQUIRE(control->children().front()->bounds().width == Catch::Approx(200));

    control->patch_style({.height = centui::px(0)});
    (void)document.prepare({400, 400, 2});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 0});
}

TEST_CASE("Refreshing a list recreates rows from current application data", "[widget][list]")
{
    std::size_t created = 0;
    auto control = centui::list{
        [&](std::size_t)
        {
            ++created;
            return centui::container{};
        },
        {
            .item_count = 100,
            .row_height = 20,
            .overscan = 0,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    (void)document.prepare({200, 100});

    auto old = control->children().front()->children().front().weak();

    control->refresh();
    (void)document.prepare({200, 100});

    REQUIRE(created == 10);
    REQUIRE(old.lock().valid() == false);
    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{0, 5});
}

TEST_CASE("Lists use existing wheel, page keys and scrollbar dragging", "[widget][list][scroll]")
{
    auto control = centui::list{
        [](std::size_t)
        {
            return centui::container{};
        },
        {
            .item_count = 100,
            .row_height = 20,
            .overscan = 0,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    (void)document.prepare({200, 100});

    document.process_event({.type = centui::input_event_type::wheel, .x = 10, .y = 10, .delta_y = 100}, {200, 100});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{5, 10});
    REQUIRE(control->request_focus());

    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::page_down}, {200, 100});

    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{10, 15});

    control->set_offset({0, 0});
    (void)document.prepare({200, 100});
    document.process_event({centui::input_event_type::pointer_pressed, 195, 10}, {200, 100});
    document.process_event({centui::input_event_type::pointer_moved, 195, 100}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 195, 100}, {200, 100});

    REQUIRE(control->offset().y == control->limit().y);
    REQUIRE(control->rendered_range().second == 100);
}

TEST_CASE("Lists preserve mounted focus and restore row order when scrolling backwards", "[widget][list][focus]")
{
    std::map<std::size_t, centui::weak_handle<>> rows;
    auto control = centui::list{
        [&](std::size_t index)
        {
            auto row = centui::pressable{centui::container{}};
            row->patch_style({.height = centui::px(20)});
            rows[index] = centui::handle<>{row}.weak();

            return row;
        },
        {
            .item_count = 100,
            .row_height = 20,
            .overscan = 0,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    (void)document.prepare({200, 100});

    control->set_offset({0, 40});
    (void)document.prepare({200, 100});

    REQUIRE(rows[3].lock()->request_focus());

    control->set_offset({0, 20});
    (void)document.prepare({200, 100});

    REQUIRE(document.focused_widget() == rows[3].lock());
    REQUIRE(rows[1].lock()->request_focus());
    REQUIRE(document.focus_next());
    REQUIRE(document.focused_widget() == rows[2].lock());

    control->set_offset({0, 1000});
    (void)document.prepare({200, 100});

    REQUIRE(rows[2].lock().valid() == false);
    REQUIRE(document.focused_widget().valid() == false);
}

TEST_CASE("Lists retry failing factories without discarding existing rows", "[widget][list]")
{
    bool fail = false;
    auto control = centui::list{
        [&](std::size_t index)
        {
            if(fail && index == 5)
            {
                throw std::runtime_error("Row failed");
            }

            return centui::container{};
        },
        {
            .item_count = 100,
            .row_height = 20,
            .overscan = 0,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    (void)document.prepare({200, 100});

    auto original = control->children().front()->children().front();

    fail = true;
    control->set_offset({0, 40});

    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::runtime_error);
    REQUIRE(original->parent().valid());

    fail = false;

    REQUIRE_NOTHROW(document.prepare({200, 100}));
    REQUIRE(control->rendered_range() == std::pair<std::size_t, std::size_t>{2, 7});
    REQUIRE(original->parent().valid() == false);
}

TEST_CASE("Lists reject invalid dimensions and missing or attached rows", "[widget][list][validation]")
{
    const auto factory = [](std::size_t)
    {
        return centui::container{};
    };

    REQUIRE_THROWS_AS(centui::list({}, {}), std::invalid_argument);

    for(auto height: {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        REQUIRE_THROWS_AS(centui::list(factory, {.row_height = height}), std::invalid_argument);
    }

    REQUIRE_THROWS_AS(
        centui::list(factory, {.item_count = 2, .row_height = std::numeric_limits<float>::max()}),
        std::invalid_argument
    );

    auto control = centui::list{
        [](std::size_t)
        {
            return centui::handle<>{};
        },
        {
            .item_count = 1,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::logic_error);

    auto child = centui::container{};
    auto parent = centui::container{child};

    auto attached = centui::list{
        [child](std::size_t)
        {
            return child;
        },
        {
            .item_count = 1,
        }
    };

    size_list(attached);

    centui::document other{attached, {.system_font_discovery = false}};
    REQUIRE_THROWS_AS(other.prepare({200, 100}), std::invalid_argument);
}

TEST_CASE("List controls save state in application data and can refresh from an input callback", "[widget][list]")
{
    std::vector<bool> checked(100, false);
    std::map<std::size_t, centui::weak_handle<centui::toggle_widget>> rows;
    centui::handle<centui::list_widget> control;

    control = centui::list{
        [&](std::size_t index)
        {
            auto row = centui::toggle{
                "Item " + std::to_string(index),
                {
                    .checked = checked[index],
                    .on_change = [&, index](bool value)
                    {
                        checked[index] = value;
                        control->refresh();
                    },
                }
            };

            row->patch_style({.height = centui::px(20)});
            rows[index] = row.weak();

            return row;
        },
        {
            .item_count = checked.size(),
            .row_height = 20,
            .overscan = 0,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    document.register_font(std::filesystem::path{CENTUI_FONT_PATH});
    REQUIRE_FALSE(document.prepare({200, 100}).commands.empty());

    document.process_event({centui::input_event_type::pointer_pressed, 10, 10}, {200, 100});
    document.process_event({centui::input_event_type::pointer_released, 10, 10}, {200, 100});

    REQUIRE(checked[0]);
    REQUIRE(rows[0].lock()->checked());

    control->scroll_to(99);
    (void)document.prepare({200, 100});

    REQUIRE_FALSE(rows[0].lock().valid());

    control->scroll_to(0);
    (void)document.prepare({200, 100});

    REQUIRE(rows[0].lock()->checked());
}

TEST_CASE("List factories cannot recursively refresh their list", "[widget][list][validation]")
{
    bool mutate = true;
    centui::handle<centui::list_widget> control;

    control = centui::list{
        [&](std::size_t)
        {
            if(mutate)
            {
                control->refresh();
            }

            return centui::container{};
        },
        {
            .item_count = 10,
        }
    };

    size_list(control);

    centui::document document{control, {.system_font_discovery = false}};
    REQUIRE_THROWS_AS(document.prepare({200, 100}), std::logic_error);

    mutate = false;

    REQUIRE_NOTHROW(document.prepare({200, 100}));
    REQUIRE(control->rendered_range().second > 0);
}
