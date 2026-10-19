#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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

namespace preparation_tests
{
struct backend
{
    centui::font                face;
    centui::viewport            size{400, 200};
    const centui::draw_command* submitted = nullptr;
    std::size_t                 count = 0;
    centui::rect                panel{};
    centui::rect                action{};
    const centui::text_layout*  second_layout = nullptr;
    std::string                 first_value;
    centui::color               first_color{};
    centui::viewport            viewport() const
    {
        return size;
    }
    centui::font& glyphs()
    {
        return face;
    }
    void draw(std::span<const centui::draw_command> commands)
    {
        submitted = commands.data();
        count = commands.size();
        bool first = true;
        for(const auto& command: commands)
        {
            if(const auto* rectangle = std::get_if<centui::rectangle_command>(&command))
            {
                panel = rectangle->bounds;
            }
            if(const auto* label = std::get_if<centui::text_command>(&command))
            {
                if(first)
                {
                    first_value = label->value;
                    first_color = label->color;
                    first = false;
                }
                if(label->value == "Second" && label->shaped)
                {
                    second_layout = &*label->shaped;
                }
                if(label->value == "Action")
                {
                    action = label->bounds;
                }
            }
        }
    }
};

struct fixture
{
    backend             renderer;
    centui::text<>      first{"First"};
    centui::text<>      second{"Second"};
    centui::button<>    action{"Action"};
    centui::container<> root{first, second, action};
    centui::document    document{root};
    centui::ui<backend> ui{renderer};

    fixture()
    {
        document.enable_preparation_diagnostics();
        root.patch_style({.padding = centui::edges::all(centui::px(10)), .gap = centui::px(5), .background_color = centui::states_color::all(centui::color::from_hex(0x123456))});
        action.patch_style({.width = centui::px(100), .height = centui::px(30)});
        ui.render(document);
    }
};
} // namespace preparation_tests

TEST_CASE("Idle frames reuse topology shaping layout and the command array", "[preparation]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    REQUIRE(before.shapings == 3);
    REQUIRE(before.layout_passes == 1);
    REQUIRE(before.paint_builds == 1);
    const auto* commands = f.renderer.submitted;
    const auto* glyphs = f.renderer.second_layout->glyphs.data();
    for(int i = 0; i < 5; ++i)
    {
        f.ui.render(f.document);
    }
    REQUIRE(f.document.preparation_stats() == before);
    REQUIRE(f.renderer.submitted == commands);
    REQUIRE(f.renderer.second_layout->glyphs.data() == glyphs);
    const auto copy = centui::prepare(f.document, f.renderer.face, 400, 200);
    REQUIRE(copy.size() == f.renderer.count);
    REQUIRE(f.document.preparation_stats() == before);
}

TEST_CASE("Color edits rebuild only paint", "[preparation]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    const auto                 color = centui::color::from_hex(0xABCDEF);
    f.first.patch_style({.text_color = centui::states_color::all(color)});
    f.ui.render(f.document);
    auto expected = before;
    ++expected.paint_builds;
    REQUIRE(f.document.preparation_stats() == expected);
    REQUIRE(f.renderer.first_color == color);
}

TEST_CASE("Text and font-size edits remeasure only the changed label", "[preparation]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    const auto*                other_layout = f.renderer.second_layout;
    const auto*                other_glyphs = other_layout->glyphs.data();
    f.first.set_text("A much wider first label");
    f.ui.render(f.document);
    const auto after = f.document.preparation_stats();
    REQUIRE(after.shapings == before.shapings + 1);
    REQUIRE(after.measurements == before.measurements + 1);
    REQUIRE(after.layout_passes == before.layout_passes + 1);
    REQUIRE(after.topology_syncs == before.topology_syncs);
    REQUIRE(f.renderer.second_layout == other_layout);
    REQUIRE(f.renderer.second_layout->glyphs.data() == other_glyphs);
    REQUIRE(f.renderer.first_value == "A much wider first label");
    f.first.patch_style({.font_size = 24});
    f.ui.render(f.document);
    REQUIRE(f.document.preparation_stats().shapings == after.shapings + 1);
    REQUIRE(f.document.preparation_stats().measurements == after.measurements + 1);
    REQUIRE(f.renderer.second_layout->glyphs.data() == other_glyphs);
}

TEST_CASE("Resize refreshes geometry without relinking or reshaping", "[preparation]")
{
    preparation_tests::fixture f;
    f.root.patch_style({.width = centui::percent(50), .height = centui::percent(50)});
    f.ui.render(f.document);
    REQUIRE(f.renderer.panel.width == 200);
    const auto before = f.document.preparation_stats();
    f.renderer.size = {800, 400};
    f.ui.render(f.document);
    auto expected = before;
    ++expected.layout_passes;
    ++expected.paint_builds;
    REQUIRE(f.document.preparation_stats() == expected);
    REQUIRE(f.renderer.panel.width == 400);
    REQUIRE(f.renderer.panel.height == 200);
}

TEST_CASE("Input and rendering share geometry while hover invalidates only paint", "[preparation][input]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    f.first.set_text("Changed before input");
    const auto                point = f.renderer.action;
    const centui::input_event move{.type = centui::input_event_type::pointer_moved, .x = point.x + 1, .y = point.y + 1};
    f.ui.process_event(f.document, move);
    const auto input = f.document.preparation_stats();
    REQUIRE(input.shapings == before.shapings + 1);
    REQUIRE(input.layout_passes == before.layout_passes + 1);
    REQUIRE(input.paint_builds == before.paint_builds);
    f.ui.render(f.document);
    auto expected = input;
    ++expected.paint_builds;
    REQUIRE(f.document.preparation_stats() == expected);
    f.ui.process_event(f.document, move);
    f.ui.render(f.document);
    REQUIRE(f.document.preparation_stats() == expected);
    f.ui.process_event(f.document, {.type = centui::input_event_type::pointer_left});
    f.ui.render(f.document);
    ++expected.paint_builds;
    REQUIRE(f.document.preparation_stats() == expected);
}

TEST_CASE("Topology edits synchronize only affected parents and retain measurements", "[preparation][document]")
{
    centui::font        face;
    centui::text        first{"First"};
    centui::text        second{"Second"};
    centui::text        other{"Other"};
    centui::container<> left{first, second};
    centui::container<> right{other};
    centui::container<> root{left, right};
    centui::document    document{root};
    document.enable_preparation_diagnostics();
    (void)centui::prepare(document, face, 400, 200);
    auto                                 before = document.preparation_stats();
    const std::array<centui::element, 2> order{second, first};
    document.reorder(left, order);
    {
        const auto commands = centui::prepare(document, face, 400, 200);
        REQUIRE(std::get<centui::text_command>(commands[0]).value == "Second");
    }
    REQUIRE(document.preparation_stats().topology_syncs == before.topology_syncs + 1);
    REQUIRE(document.preparation_stats().shapings == before.shapings);
    REQUIRE(document.preparation_stats().measurements == before.measurements);
    before = document.preparation_stats();
    document.reparent(second, right);
    {
        const auto commands = centui::prepare(document, face, 400, 200);
        REQUIRE(std::get<centui::text_command>(commands[0]).value == "First");
        REQUIRE(std::get<centui::text_command>(commands[1]).value == "Other");
        REQUIRE(std::get<centui::text_command>(commands[2]).value == "Second");
    }
    REQUIRE(document.preparation_stats().topology_syncs == before.topology_syncs + 2);
    REQUIRE(document.preparation_stats().shapings == before.shapings);
    before = document.preparation_stats();
    document.remove(second);
    REQUIRE(centui::prepare(document, face, 400, 200).size() == 2);
    REQUIRE(document.preparation_stats().topology_syncs == before.topology_syncs + 1);
    REQUIRE(document.preparation_stats().shapings == before.shapings);
    before = document.preparation_stats();
    document.append(left, centui::text{"Inserted"});
    REQUIRE(centui::prepare(document, face, 400, 200).size() == 3);
    REQUIRE(document.preparation_stats().topology_syncs == before.topology_syncs + 1);
    REQUIRE(document.preparation_stats().shapings == before.shapings + 1);
}

TEST_CASE("Text cache keys use actual values even when edits are coalesced", "[preparation]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    f.first.set_text("Transient");
    f.first.set_text("First");
    f.ui.render(f.document);
    auto expected = before;
    ++expected.paint_builds;
    REQUIRE(f.document.preparation_stats() == expected);
    REQUIRE(f.renderer.first_value == "First");
}

TEST_CASE("Font moves preserve cache identity and reused font storage invalidates it", "[preparation][font]")
{
    centui::text     label{"Font identity"};
    centui::document document{label};
    document.enable_preparation_diagnostics();
    std::optional<centui::font> face{std::in_place};
    const auto*                 address = std::addressof(*face);
    (void)centui::prepare(document, *face, 400, 200);
    const auto before = document.preparation_stats();
    auto       moved = std::move(*face);
    (void)centui::prepare(document, moved, 400, 200);
    REQUIRE(document.preparation_stats() == before);
    *face = std::move(moved);
    face.reset();
    face.emplace();
    REQUIRE(std::addressof(*face) == address);
    (void)centui::prepare(document, *face, 400, 200);
    REQUIRE(document.preparation_stats().shapings == before.shapings + 1);
    // Replacing the same default face changes resource identity, but its
    // identical intrinsic dimensions do not require another Yoga measurement.
    REQUIRE(document.preparation_stats().measurements == before.measurements);
    REQUIRE(document.preparation_stats().layout_passes == before.layout_passes);
}

TEST_CASE("Preparation failure leaves pending work recoverable", "[preparation]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    f.first.set_text("Retry succeeds");
    auto saved = std::move(f.renderer.face);
    REQUIRE_THROWS_AS(centui::prepare(f.document, f.renderer.face, 400, 200), std::logic_error);
    REQUIRE(f.first.pending_changes().text);
    REQUIRE(f.document.preparation_stats() == before);
    f.renderer.face = std::move(saved);
    f.ui.render(f.document);
    REQUIRE(f.document.preparation_stats().shapings == before.shapings + 1);
    REQUIRE(f.document.preparation_stats().layout_passes == before.layout_passes + 1);
    REQUIRE(f.renderer.first_value == "Retry succeeds");
    REQUIRE_FALSE(f.first.pending_changes().text);
}

TEST_CASE("Construction trees prepare incrementally and keep shaping through mounting", "[preparation]")
{
    centui::font        face;
    centui::text        first{"First"};
    centui::text        second{"Second"};
    centui::container<> root{first, second};
    root.enable_preparation_diagnostics();
    (void)centui::prepare(root, face, 400, 200);
    const auto before = root.preparation_stats();
    (void)centui::prepare(root, face, 400, 200);
    REQUIRE(root.preparation_stats() == before);
    centui::document document{root};
    const auto       commands = centui::prepare(document, face, 400, 200);
    REQUIRE(commands.size() == 2);
    REQUIRE(document.preparation_stats() == before);
}

TEST_CASE("Preparing a subtree shares owner geometry and returns its command range", "[preparation]")
{
    preparation_tests::fixture f;
    const auto                 before = f.document.preparation_stats();
    const auto                 commands = centui::prepare(f.second, f.renderer.face, 400, 200);
    REQUIRE(commands.size() == 1);
    REQUIRE(std::get<centui::text_command>(commands[0]).value == "Second");
    REQUIRE(std::get<centui::text_command>(commands[0]).bounds.y > 10);
    REQUIRE(f.document.preparation_stats() == before);
    REQUIRE(f.second.preparation_stats() == before);
}

TEST_CASE("Preparation diagnostics are optional and observe only work after enabling", "[preparation][diagnostics]")
{
    centui::font     face;
    centui::text     label{"Initial"};
    centui::document document{label};
    (void)centui::prepare(document, face, 400, 200);
    REQUIRE(document.preparation_stats() == centui::preparation_statistics{});

    document.enable_preparation_diagnostics();
    (void)centui::prepare(document, face, 400, 200);
    REQUIRE(document.preparation_stats() == centui::preparation_statistics{});

    label.set_text("Observed");
    (void)centui::prepare(document, face, 400, 200);
    REQUIRE(document.preparation_stats().shapings == 1);
    REQUIRE(document.preparation_stats().layout_passes == 1);

    document.enable_preparation_diagnostics(false);
    label.set_text("Unobserved");
    (void)centui::prepare(document, face, 400, 200);
    REQUIRE(document.preparation_stats() == centui::preparation_statistics{});
    REQUIRE(std::get<centui::text_command>(centui::prepare(document, face, 400, 200)[0]).value == "Unobserved");

    document.enable_preparation_diagnostics();
    REQUIRE(document.preparation_stats() == centui::preparation_statistics{});
}

TEST_CASE("Mounting transfers a prepared runtime without moving text or rebuilding paint", "[preparation][document]")
{
    centui::font        face;
    centui::text        label{"Stable"};
    centui::container<> root{label};
    root.enable_preparation_diagnostics();
    const auto  before = centui::prepare(root, face, 400, 200);
    const auto  statistics = root.preparation_stats();
    const auto& original = std::get<centui::text_command>(before[0]);
    const auto* text = original.value.data();
    const auto* shaped = &*original.shaped;

    {
        centui::document document{root};
        const auto       after = centui::prepare(document, face, 400, 200);
        const auto&      mounted = std::get<centui::text_command>(after[0]);
        REQUIRE(document.preparation_stats() == statistics);
        REQUIRE(mounted.value.data() == text);
        REQUIRE(&*mounted.shaped == shaped);
        REQUIRE(document.children(root)[0] == label);
    }
    REQUIRE_FALSE(root.valid());
    REQUIRE_FALSE(label.valid());
}

TEST_CASE("Composition transfers prepared services without reshaping descendants", "[preparation][container]")
{
    centui::font        face;
    centui::text        label{"Prepared child"};
    centui::container<> child{label};
    const auto          initial = centui::prepare(child, face, 400, 200);
    const auto&         command = std::get<centui::text_command>(initial[0]);
    const auto*         shaped = &*command.shaped;
    const auto*         text = command.value.data();

    centui::container<> root{child};
    root.enable_preparation_diagnostics();
    const auto result = centui::prepare(root, face, 400, 200);
    REQUIRE(root.preparation_stats().shapings == 0);
    REQUIRE(root.preparation_stats().measurements == 0);
    REQUIRE(root.preparation_stats().topology_syncs == 1);
    REQUIRE(std::get<centui::text_command>(result[0]).value.data() == text);
    REQUIRE(&*std::get<centui::text_command>(result[0]).shaped == shaped);

    centui::document document{root};
    label.set_text("Still the same widget");
    REQUIRE(document.children(child)[0] == label);
    REQUIRE(std::get<centui::text_command>(centui::prepare(document, face, 400, 200)[0]).value == "Still the same widget");
}

TEST_CASE("Rejected composition preserves prepared input trees", "[preparation][container]")
{
    centui::font        face;
    centui::text        label{"Prepared"};
    centui::container<> child{label};
    child.enable_preparation_diagnostics();
    (void)centui::prepare(child, face, 400, 200);
    const auto before = child.preparation_stats();

    REQUIRE_THROWS_AS((centui::container<>{child, child}), std::invalid_argument);
    REQUIRE(child.valid());
    REQUIRE_FALSE(child.mounted());
    const auto commands = centui::prepare(child, face, 400, 200);
    REQUIRE(child.preparation_stats() == before);
    REQUIRE(std::get<centui::text_command>(commands[0]).value == "Prepared");
}

TEST_CASE("Reused slots cannot reuse the previous widget's text and layout caches", "[preparation][document]")
{
    centui::font face;
    centui::text previous{"Same text"};
    previous.patch_style({.font_size = 30});
    centui::container<> root{previous};
    centui::document    document{root};
    document.enable_preparation_diagnostics();
    (void)centui::prepare(document, face, 400, 200);
    const auto old_id = previous.id();
    document.remove(previous);

    centui::text next{"Same text"};
    document.append(root, next);
    REQUIRE(next.id().index == old_id.index);
    REQUIRE(next.id().generation != old_id.generation);
    const auto before = document.preparation_stats();
    const auto commands = centui::prepare(document, face, 400, 200);
    REQUIRE(document.preparation_stats().shapings == before.shapings + 1);
    REQUIRE(document.preparation_stats().measurements == before.measurements + 1);
    REQUIRE(std::get<centui::text_command>(commands[0]).font_size == 16);
    REQUIRE_FALSE(previous.valid());
}

TEST_CASE("Subtree command ranges exclude later siblings at every depth", "[preparation][paint]")
{
    centui::font        face;
    centui::text        first{"First"};
    centui::text        second{"Second"};
    centui::container<> nested{first, second};
    centui::text        third{"Third"};
    centui::container<> branch{nested, third};
    centui::text        last{"Last"};
    centui::container<> root{branch, last};
    centui::document    document{root};
    document.enable_preparation_diagnostics();
    REQUIRE(centui::prepare(document, face, 400, 200).size() == 4);
    const auto before = document.preparation_stats();

    const auto branch_commands = centui::prepare(branch, face, 400, 200);
    REQUIRE(branch_commands.size() == 3);
    REQUIRE(std::get<centui::text_command>(branch_commands.back()).value == "Third");
    const auto nested_commands = centui::prepare(nested, face, 400, 200);
    REQUIRE(nested_commands.size() == 2);
    REQUIRE(std::get<centui::text_command>(nested_commands.back()).value == "Second");
    REQUIRE(document.preparation_stats() == before);
}

TEST_CASE("Preparation rejects moved and expired handles before accessing runtime identity", "[preparation][lifetime]")
{
    centui::font face;
    centui::text label{"Moved"};
    auto         moved = std::move(label);
    REQUIRE_THROWS_AS(centui::prepare(label, face, 400, 200), centui::invalid_element);

    {
        centui::document document{moved};
        REQUIRE(centui::prepare(document, face, 400, 200).size() == 1);
    }
    REQUIRE_THROWS_AS(centui::prepare(moved, face, 400, 200), centui::invalid_element);
}
