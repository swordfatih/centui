#include <functional>

#include <centui/centui.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

centui::style box(float width, float height, float x = 0, float y = 0)
{
    centui::style value;
    value.position = centui::position::absolute;
    value.inset.left = centui::px(x);
    value.inset.top = centui::px(y);
    value.width = centui::px(width);
    value.height = centui::px(height);
    return value;
}

class reveal_probe : public centui::widget
{
public:
    reveal_probe()
    {
        set_focusable();
        set_interactive();
    }

    using widget::request_reveal;
    std::function<void()> geometry;
    std::function<void(bool)> focus;

protected:
    void on_focus_changed(bool value) override
    {
        if(focus)
        {
            focus(value);
        }
    }

    void on_geometry() override
    {
        if(geometry)
        {
            geometry();
        }
    }
};

class counting_scroll : public centui::scroll_widget
{
public:
    using scroll_widget::scroll_widget;
    int reveals = 0;
    std::function<void()> disrupt;

protected:
    centui::vec2 reveal_descendant(centui::rect target) override
    {
        ++reveals;
        const auto applied = scroll_widget::reveal_descendant(target);
        if(disrupt)
        {
            disrupt();
        }
        return applied;
    }
};

struct reveal_fixture
{
    centui::handle<reveal_probe> target = centui::make_widget<reveal_probe>();
    centui::container<> content{box(400, 400)};
    centui::handle<counting_scroll> scroll = centui::make_widget<counting_scroll>();
    centui::document doc;

    reveal_fixture()
    {
        target->set_style(box(20, 20, 250, 260));
        content->append(target);
        scroll->patch_style({.width = centui::px(100), .height = centui::px(100)});
        scroll->append(content);
        doc.set_root(scroll);
        (void)doc.prepare({100, 100});
    }
};

} // namespace

TEST_CASE("Reveal applies minimum movement on both axes and retains logical focus", "[focus][reveal]")
{
    reveal_fixture f;
    const auto before = f.doc.statistics();
    REQUIRE(f.target->request_focus());
    REQUIRE(f.scroll->offset().x == Catch::Approx(170));
    REQUIRE(f.scroll->offset().y == Catch::Approx(180));
    REQUIRE(f.target->focused());
    REQUIRE(f.target->bounds().x == Catch::Approx(80));
    REQUIRE(f.target->bounds().y == Catch::Approx(80));
    REQUIRE(f.doc.statistics().measurement_calls == before.measurement_calls);
    REQUIRE(f.doc.statistics().layout_calculations == before.layout_calculations);
    const auto offset = f.scroll->offset();
    f.target->reveal();
    REQUIRE(f.scroll->offset() == offset);
    f.scroll->set_offset({0, 0});
    (void)f.doc.prepare({100, 100});
    REQUIRE(f.scroll->offset() == centui::vec2{}); // No focus snap-back on paint.
    f.doc.process_event({centui::input_event_type::focus_lost}, {100, 100});
    f.doc.process_event({centui::input_event_type::focus_gained}, {100, 100});
    REQUIRE(f.scroll->offset() == centui::vec2{});
    f.target->reveal();
    f.doc.process_event({centui::input_event_type::pointer_moved, 85, 85}, {100, 100});
    REQUIRE(f.target->hovered());
}

TEST_CASE("Explicit reveal coalesces to the latest local rectangle after geometry", "[focus][reveal]")
{
    reveal_fixture f;
    f.target->geometry = [&]
    {
        f.target->request_reveal({0, 0, 10, 10});
        f.target->request_reveal({50, 60, 10, 10});
    };
    f.target->patch_style({.inset = centui::edges{.top = centui::px(210), .left = centui::px(200)}});
    (void)f.doc.prepare({100, 100});
    REQUIRE(f.scroll->offset().x == Catch::Approx(160));
    REQUIRE(f.scroll->offset().y == Catch::Approx(180));
    REQUIRE_FALSE(f.doc.pending_changes().any());
}

TEST_CASE("Reveal clamps to actual ranges and a non-scrolling clip keeps focus", "[focus][reveal]")
{
    reveal_fixture f;
    f.target->request_reveal({1000, 1000, 20, 20});
    REQUIRE(f.scroll->offset() == f.scroll->limit());
    f.content->patch_style({.width = centui::px(20), .height = centui::px(20)});
    f.target->patch_style({.inset = centui::edges::all(centui::px(0))});
    (void)f.doc.prepare({100, 100});
    f.target->reveal();
    REQUIRE(f.scroll->offset() == centui::vec2{});
    REQUIRE(f.scroll->limit() == centui::vec2{});

    auto target = centui::make_widget<reveal_probe>();
    target->set_style(box(20, 20, 0, 200));
    auto clip = centui::container{{target}, box(100, 40)};
    clip->patch_style({.overflow = centui::overflow::clip});
    centui::document doc{clip};
    REQUIRE(target->request_focus());
    (void)doc.prepare({100, 100});
    REQUIRE(target->focused());
    REQUIRE(target->bounds().y == Catch::Approx(200));
    clip->patch_style({.overflow = centui::overflow::scroll});
    target->reveal();
    REQUIRE(target->bounds().y == Catch::Approx(200)); // Styling alone is not a scroll capability.
}

TEST_CASE("Nested reveal reaches an inner scroller initially outside the outer viewport", "[focus][reveal][nested]")
{
    auto target = centui::make_widget<reveal_probe>();
    target->set_style(box(20, 20, 0, 220));
    auto inside = centui::container{{target}, box(100, 300)};
    auto inner = centui::make_widget<counting_scroll>();
    inner->set_style(box(100, 80, 0, 250));
    inner->patch_style({.overflow = centui::overflow::scroll});
    inner->append(inside);
    auto outside = centui::container{{inner}, box(100, 500)};
    auto outer = centui::make_widget<counting_scroll>();
    outer->patch_style({.width = centui::px(100), .height = centui::px(100)});
    outer->append(outside);
    centui::document doc{outer};
    (void)doc.prepare({100, 100});
    REQUIRE(target->request_focus());
    REQUIRE(inner->offset().y == Catch::Approx(160));
    REQUIRE(outer->offset().y == Catch::Approx(230));
    REQUIRE(target->bounds().y == Catch::Approx(80));
    REQUIRE(inner->reveals == 1);
    REQUIRE(outer->reveals == 1);
}

TEST_CASE("Fixed clipping between scrollers carries only reachable portions", "[focus][reveal][nested]")
{
    for(float target_height: {20.0f, 50.0f})
    {
        DYNAMIC_SECTION("target height " << target_height)
        {
            auto target = centui::make_widget<reveal_probe>();
            target->set_style(box(20, target_height, 0, 150));
            auto inside = centui::container{{target}, box(100, 300)};
            auto inner = centui::make_widget<counting_scroll>();
            inner->set_style(box(100, 80));
            inner->patch_style({.overflow = centui::overflow::scroll});
            inner->append(inside);
            auto fixed_clip = centui::container{{inner}, box(100, 40, 0, 250)};
            fixed_clip->patch_style({.overflow = centui::overflow::clip});
            auto outside = centui::container{{fixed_clip}, box(100, 500)};
            auto outer = centui::scroll{{outside}};
            outer->patch_style({.width = centui::px(100), .height = centui::px(100)});
            centui::document doc{outer};
            (void)doc.prepare({100, 100});
            REQUIRE(target->request_focus());
            REQUIRE(target->focused());
            REQUIRE(inner->reveals == 1);
            REQUIRE(outer->offset().y == Catch::Approx(target_height == 20 ? 0 : 190));
            const auto offset = outer->offset();
            (void)doc.prepare({100, 100});
            REQUIRE(outer->offset() == offset);
        }
    }
}

TEST_CASE("Oversized targets choose stable edges without oscillation", "[focus][reveal]")
{
    reveal_fixture f;
    f.target->set_style(box(150, 150, 200, 200));
    f.target->reveal();
    REQUIRE(f.scroll->offset() == centui::vec2{200, 200});
    f.target->reveal();
    REQUIRE(f.scroll->offset() == centui::vec2{200, 200});
    f.scroll->set_offset({250, 250});
    f.target->reveal(); // Oversized target now covers viewport: no movement.
    REQUIRE(f.scroll->offset() == centui::vec2{250, 250});
    f.target->set_style(box(150, 150));
    f.target->reveal(); // Trailing edge aligns when the target lies before viewport.
    REQUIRE(f.scroll->offset() == centui::vec2{50, 50});
}

TEST_CASE("Zero surface and local viewports park reveal until geometry becomes usable", "[focus][reveal]")
{
    reveal_fixture f;
    (void)f.doc.prepare({0, 0});
    REQUIRE(f.target->request_focus());
    REQUIRE(f.target->focused());
    REQUIRE(f.scroll->reveals == 0);
    (void)f.doc.prepare({100, 100});
    REQUIRE(f.scroll->offset().y == Catch::Approx(180));

    f.scroll->set_offset({0, 0});
    f.scroll->patch_style({.height = centui::px(0)});
    f.target->reveal();
    const auto count = f.scroll->reveals;
    (void)f.doc.prepare({100, 100});
    REQUIRE(f.scroll->reveals == count);
    f.scroll->patch_style({.height = centui::px(100)});
    (void)f.doc.prepare({100, 100});
    REQUIRE(f.scroll->offset().y == Catch::Approx(180));
}

TEST_CASE("Removed reveal targets are discarded and topology restarts use settled ancestors", "[focus][reveal]")
{
    reveal_fixture f;
    bool moved = false;
    f.scroll->disrupt = [&]
    {
        if(!std::exchange(moved, true))
        {
            f.doc.remove(f.target);
        }
    };
    f.target->reveal();
    REQUIRE_FALSE(f.target->parent().valid());
    (void)f.doc.prepare({100, 100});
    REQUIRE_FALSE(f.doc.pending_changes().any());
    f.scroll->disrupt = {};
    auto wrapper = centui::container{{f.target}, box(300, 300)};
    f.content->append(wrapper);
    f.target->reveal();
    REQUIRE(f.target->bounds().y + f.target->bounds().height <= 100);
}

TEST_CASE("Geometry disruptions restart reveal at most eight times", "[focus][reveal]")
{
    reveal_fixture f;
    f.scroll->disrupt = [&]
    {
        f.target->patch_style(
            {.inset = centui::edges{.top = centui::px(260), .left = centui::px(250 + f.scroll->reveals)}}
        );
    };
    f.target->reveal();
    REQUIRE(f.scroll->reveals == 8);
    REQUIRE_FALSE(f.doc.pending_changes().any());
    f.scroll->disrupt = {};
    f.target->reveal();
    REQUIRE(f.scroll->reveals == 9);
}

TEST_CASE("Reveal restarts when deferred work changes the surviving target's ancestor chain", "[focus][reveal]")
{
    reveal_fixture f;
    REQUIRE(f.target->request_focus());
    f.scroll->set_offset({0, 0});
    f.scroll->reveals = 0;
    bool removed = false;
    f.target->focus = [&](bool focused)
    {
        if(!focused)
        {
            auto wrapper = centui::container{{f.target}, box(300, 300)};
            f.content->append(wrapper);
        }
    };
    f.scroll->disrupt = [&]
    {
        if(!std::exchange(removed, true))
        {
            f.doc.remove(f.target);
        }
    };
    f.target->reveal(); // Explicit requests survive logical focus loss.
    REQUIRE(f.target->parent() != centui::handle<>{f.content});
    REQUIRE_FALSE(f.target->focused());
    REQUIRE(f.scroll->reveals == 2);
    REQUIRE(f.target->bounds().y + f.target->bounds().height <= 100);
}
