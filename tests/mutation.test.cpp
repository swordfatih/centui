#include <array>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import centui.document;
import centui.container;
import centui.text;
import centui.button;
import centui.color;
import centui.unit;
import centui.glyph;
import centui.draw;
import centui.ui;
import centui.input;

static_assert(std::same_as<decltype(std::declval<centui::text<>&>().style()), const centui::style&>);

namespace mutation_tests
{
struct backend
{
    centui::font               face;
    centui::rect               action_bounds{};
    const centui::text_layout* label_layout = nullptr;
    centui::viewport           viewport() const
    {
        return {400, 200};
    }
    centui::font& glyphs()
    {
        return face;
    }
    void draw(std::span<const centui::draw_command> commands)
    {
        for(const auto& command: commands)
        {
            if(const auto* text = std::get_if<centui::text_command>(&command))
            {
                if(text->value == "Action")
                {
                    action_bounds = text->bounds;
                }
                if(text->value == "Before" && text->shaped)
                {
                    label_layout = &*text->shaped;
                }
            }
        }
    }
};

struct fixture
{
    backend             renderer;
    centui::text<>      label{"Before"};
    centui::button<>    action{"Action"};
    centui::container<> root{label, action};
    centui::document    document{root};
    centui::ui<backend> ui{renderer};

    fixture()
    {
        settle();
    }
    void settle()
    {
        ui.process_event(document, {.type = centui::input_event_type::pointer_left});
        ui.render(document);
    }
};
} // namespace mutation_tests

TEST_CASE("Text changes invalidate their measurement and ancestor geometry", "[mutation]")
{
    mutation_tests::fixture f;
    REQUIRE_FALSE(f.document.pending_changes().any());
    f.label.set_text("Changed");
    REQUIRE(f.label.pending_changes() == centui::invalidation{.text = true, .layout = true, .paint = true});
    REQUIRE(f.root.pending_changes() == centui::invalidation{.layout = true, .paint = true});
    REQUIRE_FALSE(f.action.pending_changes().any());
    REQUIRE(f.document.pending_changes() == f.label.pending_changes());
    f.settle();
    REQUIRE_FALSE(f.document.pending_changes().any());
    f.action.set_text("New button label");
    REQUIRE(f.action.value() == "New button label");
    REQUIRE(f.action.pending_changes() == centui::invalidation{.text = true, .layout = true, .paint = true});
}

TEST_CASE("Style patches distinguish layout text and paint changes", "[mutation][style]")
{
    mutation_tests::fixture f;
    f.label.patch_style({.width = centui::px(120)});
    REQUIRE(f.label.pending_changes() == centui::invalidation{.layout = true, .paint = true});
    REQUIRE(f.label.style().width == centui::px(120));
    f.settle();

    REQUIRE(f.renderer.label_layout != nullptr);
    const auto* glyphs = f.renderer.label_layout->glyphs.data();
    const auto  id = f.label.id();
    f.label.patch_style({.text_color = centui::states_color::all(centui::color::from_hex(0x123456))});
    REQUIRE(f.label.pending_changes() == centui::invalidation{.paint = true});
    REQUIRE(f.root.pending_changes() == centui::invalidation{.paint = true});
    REQUIRE(f.document.pending_changes() == centui::invalidation{.paint = true});
    REQUIRE(f.renderer.label_layout->glyphs.data() == glyphs);
    REQUIRE(f.label.id() == id);
    REQUIRE(f.document.children(f.root)[0] == f.label);
    f.settle();

    f.label.patch_style({.font_size = 24});
    REQUIRE(f.label.pending_changes() == centui::invalidation{.text = true, .layout = true, .paint = true});
    f.settle();
    f.label.patch_style({.text_align = centui::text_alignment::center});
    REQUIRE(f.label.pending_changes() == centui::invalidation{.paint = true});
}

TEST_CASE("Invalid patches preserve every property and existing dirty state", "[mutation][style]")
{
    mutation_tests::fixture f;
    const auto              before = f.label.style();
    const auto              pending = f.document.pending_changes();
    const auto*             glyphs = f.renderer.label_layout->glyphs.data();
    REQUIRE_THROWS_AS(f.label.patch_style({.width = centui::px(200), .font_size = 0}), std::invalid_argument);
    REQUIRE(f.label.style() == before);
    REQUIRE(f.document.pending_changes() == pending);
    REQUIRE(f.renderer.label_layout->glyphs.data() == glyphs);

    f.label.set_text("Pending edit");
    const auto dirty = f.document.pending_changes();
    REQUIRE_THROWS_AS(f.label.patch_style({.font_size = -1}), std::invalid_argument);
    REQUIRE(f.label.style() == before);
    REQUIRE(f.label.value() == "Pending edit");
    REQUIRE(f.document.pending_changes() == dirty);
    auto invalid = before;
    invalid.height = centui::px(-20);
    REQUIRE_THROWS_AS(f.label.set_style(invalid), std::invalid_argument);
    REQUIRE(f.label.style() == before);
    REQUIRE(f.document.pending_changes() == dirty);
}

TEST_CASE("Style validation covers lengths font sizes and enum domains", "[mutation][style]")
{
    centui::text label{"Draft"};
    const auto   before = label.style();
    const auto   pending = label.pending_changes();
    for(float bad: {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        REQUIRE_THROWS_AS(label.patch_style({.width = centui::px(bad)}), std::invalid_argument);
    }
    for(float bad: {0.0f, -1.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::quiet_NaN()})
    {
        REQUIRE_THROWS_AS(label.patch_style({.font_size = bad}), std::invalid_argument);
    }
    REQUIRE_THROWS_AS(label.patch_style({.padding = centui::edges::all(centui::px(-1))}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.gap = centui::percent(-1)}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.width = centui::length{1, centui::unit::fit}}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.width = centui::length{0, static_cast<centui::unit>(99)}}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.position = static_cast<centui::position>(99)}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.flex_direction = static_cast<centui::direction>(99)}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.align_items = static_cast<centui::alignment>(99)}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.justify_content = static_cast<centui::justification>(99)}), std::invalid_argument);
    REQUIRE_THROWS_AS(label.patch_style({.text_align = static_cast<centui::text_alignment>(99)}), std::invalid_argument);
    REQUIRE(label.style() == before);
    REQUIRE(label.pending_changes() == pending);
    label.patch_style({.margin = centui::edges::all(centui::px(-5)), .inset = centui::edges::all(centui::percent(-10))});
    REQUIRE(label.style().margin.left == centui::px(-5));
}

TEST_CASE("Compile time styles and runtime patches use the same values and validation", "[mutation][style]")
{
    struct initial_style
    {
        centui::length width = centui::px(100);
        float          font_size = 20;
    };
    constexpr auto              initial = centui::style::resolve<initial_style>();
    centui::text<initial_style> constructed{"Initial"};
    centui::text                patched{"Patched"};
    patched.patch_style({.width = centui::px(100), .font_size = 20});
    REQUIRE(constructed.style() == initial);
    REQUIRE(patched.style() == initial);
    REQUIRE_THROWS_AS(constructed.patch_style({.font_size = 0}), std::invalid_argument);
    REQUIRE(constructed.style() == initial);
    centui::document document{constructed};
    REQUIRE_THROWS_AS(constructed.patch_style({.font_size = 0}), std::invalid_argument);
    REQUIRE(constructed.style() == initial);
}

TEST_CASE("Equal property writes and unchanged child orders do not invalidate", "[mutation]")
{
    mutation_tests::fixture f;
    f.label.value("Before");
    f.action.set_text("Action");
    f.action.set_enabled(true);
    f.action.set_on_click({});
    f.label.set_style(f.label.style());
    f.label.patch_style({});
    f.root.patch_style({.width = f.root.style().width});
    const auto children = f.document.children(f.root);
    f.document.reorder(f.root, children);
    f.document.reparent(f.action, f.root, 1);
    REQUIRE_FALSE(f.document.pending_changes().any());
}

TEST_CASE("Enabled changes invalidate eligibility and paint and cancel capture", "[mutation][input]")
{
    mutation_tests::fixture f;
    int                     clicks = 0;
    f.action.set_on_click([&]
    {
        ++clicks;
    });
    REQUIRE(f.action.pending_changes() == centui::invalidation{.interaction = true});
    f.settle();
    const auto bounds = f.renderer.action_bounds;
    f.ui.process_event(f.document, {.type = centui::input_event_type::pointer_pressed, .x = bounds.x + 1, .y = bounds.y + 1});
    f.ui.render(f.document);
    REQUIRE_FALSE(f.document.pending_changes().any());
    f.action.set_enabled(false);
    REQUIRE(f.action.pending_changes() == centui::invalidation{.paint = true, .interaction = true});
    REQUIRE_FALSE(f.action.enabled());
    REQUIRE_FALSE(f.document.pending_changes().layout);
    REQUIRE_FALSE(f.document.pending_changes().text);
    f.action.set_enabled(true);
    f.ui.process_event(f.document, {.type = centui::input_event_type::pointer_released, .x = bounds.x + 1, .y = bounds.y + 1});
    REQUIRE(clicks == 0);
}

TEST_CASE("Topology changes invalidate parents and rejected operations preserve clean state", "[mutation][document]")
{
    mutation_tests::fixture f;
    REQUIRE_THROWS_AS(f.document.append(f.root, f.label), std::invalid_argument);
    REQUIRE_THROWS_AS(f.document.reparent(f.root, f.root), std::invalid_argument);
    REQUIRE_FALSE(f.document.pending_changes().any());
    centui::text inserted{"Inserted"};
    f.document.append(f.root, inserted);
    REQUIRE(f.root.pending_changes() == centui::invalidation{.topology = true, .layout = true, .paint = true, .interaction = true});
    REQUIRE(inserted.pending_changes().text);
    f.settle();
    f.document.remove(inserted);
    REQUIRE_FALSE(inserted.valid());
    REQUIRE(f.document.pending_changes() == centui::invalidation{.topology = true, .layout = true, .paint = true, .interaction = true});
    f.settle();
    f.document.remove(f.root);
    REQUIRE(f.document.empty());
    REQUIRE(f.document.pending_changes().topology);
    REQUIRE(f.document.pending_changes().interaction);
    f.settle();
    REQUIRE_FALSE(f.document.pending_changes().any());
}

TEST_CASE("Reparenting invalidates both ancestor paths without reshaping the moved widget", "[mutation][document]")
{
    mutation_tests::backend renderer;
    centui::text            label{"Moved"};
    centui::container<>     left{label};
    centui::container<>     right;
    centui::container<>     root{left, right};
    centui::document        document{root};
    centui::ui              ui{renderer};
    ui.process_event(document, {.type = centui::input_event_type::pointer_left});
    ui.render(document);
    REQUIRE_FALSE(document.pending_changes().any());
    const auto id = label.id();
    document.reparent(label, right);
    const centui::invalidation topology{.topology = true, .layout = true, .paint = true, .interaction = true};
    REQUIRE(left.pending_changes() == topology);
    REQUIRE(right.pending_changes() == topology);
    REQUIRE(root.pending_changes() == centui::invalidation{.layout = true, .paint = true});
    REQUIRE_FALSE(label.pending_changes().text);
    REQUIRE(label.id() == id);
    REQUIRE(document.parent(label).value() == right);
}

TEST_CASE("Documents and draft handles reject access from another thread", "[mutation][thread]")
{
    mutation_tests::fixture f;
    centui::text            draft{"Draft"};
    bool                    document_rejected = false;
    bool                    text_rejected = false;
    bool                    style_rejected = false;
    bool                    draft_rejected = false;
    bool                    topology_rejected = false;
    bool                    move_rejected = false;
    std::thread             worker([&]
    {
        try
        {
            (void)f.document.empty();
        }
        catch(const std::logic_error&)
        {
            document_rejected = true;
        }
        try
        {
            f.label.value("Wrong thread");
        }
        catch(const std::logic_error&)
        {
            text_rejected = true;
        }
        try
        {
            f.label.patch_style({.font_size = 24});
        }
        catch(const std::logic_error&)
        {
            style_rejected = true;
        }
        try
        {
            draft.set_text("Wrong thread");
        }
        catch(const std::logic_error&)
        {
            draft_rejected = true;
        }
        try
        {
            f.document.remove(f.label);
        }
        catch(const std::logic_error&)
        {
            topology_rejected = true;
        }
        try
        {
            centui::document moved{std::move(f.document)};
        }
        catch(const std::logic_error&)
        {
            move_rejected = true;
        }
    });
    worker.join();
    REQUIRE(document_rejected);
    REQUIRE(text_rejected);
    REQUIRE(style_rejected);
    REQUIRE(draft_rejected);
    REQUIRE(topology_rejected);
    REQUIRE(move_rejected);
    REQUIRE(f.label.value() == "Before");
    REQUIRE(draft.value() == "Draft");
    REQUIRE_FALSE(f.document.pending_changes().any());
}

TEST_CASE("Mutable click captures persist across events and event batches", "[mutation][input]")
{
    mutation_tests::fixture f;
    int                     observed = 0;
    f.action.set_on_click([label = f.label, counter = 0, &observed]() mutable
    {
        observed = ++counter;
        label.set_text(std::to_string(counter));
    });

    const auto                bounds = f.renderer.action_bounds;
    const centui::input_event press{.type = centui::input_event_type::pointer_pressed, .x = bounds.x + 1, .y = bounds.y + 1};
    const centui::input_event release{.type = centui::input_event_type::pointer_released, .x = bounds.x + 1, .y = bounds.y + 1};
    for(int expected = 1; expected <= 2; ++expected)
    {
        f.ui.process_event(f.document, press);
        f.ui.process_event(f.document, release);
        REQUIRE(observed == expected);
        REQUIRE(f.label.value() == std::to_string(expected));
    }

    const std::array events{press, release, press, release};
    f.ui.process_events(f.document, events);
    REQUIRE(observed == 4);
    REQUIRE(f.label.value() == "4");
}

TEST_CASE("A click callback can replace itself and finish using its captured state", "[mutation][input]")
{
    mutation_tests::fixture f;
    int                     observed = 0;
    f.action.set_on_click([&f, &observed, counter = 0]() mutable
    {
        f.action.set_on_click([&observed, counter = 10]() mutable
        {
            observed = ++counter;
        });
        observed = ++counter;
    });

    const auto       bounds = f.renderer.action_bounds;
    const std::array events{
        centui::input_event{.type = centui::input_event_type::pointer_pressed, .x = bounds.x + 1, .y = bounds.y + 1},
        centui::input_event{.type = centui::input_event_type::pointer_released, .x = bounds.x + 1, .y = bounds.y + 1},
    };
    f.ui.process_events(f.document, events);
    REQUIRE(observed == 1);
    f.ui.process_events(f.document, events);
    REQUIRE(observed == 11);
    f.ui.process_events(f.document, events);
    REQUIRE(observed == 12);
}
