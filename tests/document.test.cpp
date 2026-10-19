#include <array>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import centui.document;
import centui.container;
import centui.text;
import centui.button;
import centui.glyph;
import centui.draw;
import centui.unit;
import centui.color;
import centui.ui;
import centui.input;

TEST_CASE("Named widgets alias their composed and mounted identity", "[document]")
{
    centui::text title{"Before"};
    centui::button action{"Action"};
    auto alias = title;
    centui::container root{title, action};
    REQUIRE(title.valid());
    REQUIRE_FALSE(title.mounted());
    alias.value("During construction");
    REQUIRE(title.value() == "During construction");

    centui::document document{root};
    REQUIRE(title.mounted());
    REQUIRE(alias.id() == title.id());
    title.value("Mounted");
    action.set_enabled(false);
    auto children = document.children(root);
    REQUIRE(children.size() == 2);
    REQUIRE(children[0].as<centui::text<>>().value() == "Mounted");
    REQUIRE_FALSE(children[1].as<centui::button<>>().enabled());
    REQUIRE(document.parent(title).value() == root);
    REQUIRE_FALSE(document.parent(root));
    REQUIRE_THROWS_AS(children[0].as<centui::button<>>(), std::invalid_argument);
}

TEST_CASE("Expired document handles fail and slots never revive old identities", "[document]")
{
    centui::container root;
    centui::document document{root};
    centui::text old{"Old"};
    document.append(root, old);
    const auto id = old.id();
    document.remove(old);
    REQUIRE_FALSE(old.valid());
    REQUIRE_THROWS_AS(old.value(), centui::invalid_element);

    centui::text next{"Next"};
    document.append(root, next);
    REQUIRE(next.id().index == id.index);
    REQUIRE(next.id().generation != id.generation);
    REQUIRE_FALSE(old.valid());
    REQUIRE_THROWS_AS(document.remove(old), centui::invalid_element);
    REQUIRE(document.children(root).size() == 1);
}

TEST_CASE("Document destruction invalidates copied typed handles", "[document]")
{
    centui::text title{"Owned"};
    auto alias = title;
    {
        centui::document document{title};
        REQUIRE(alias.valid());
    }
    REQUIRE_FALSE(title.valid());
    REQUIRE_FALSE(alias.valid());
    REQUIRE_THROWS_AS(alias.value("Expired"), centui::invalid_element);

    centui::button action{"Owned"};
    { centui::document document{action}; }
    REQUIRE_THROWS_AS(action.set_enabled(false), centui::invalid_element);
}

TEST_CASE("Unmounted child handles expire with their construction owner", "[document]")
{
    centui::text title{"Child"};
    {
        centui::container<> root{title};
        REQUIRE(title.valid());
    }
    REQUIRE_FALSE(title.valid());
    REQUIRE_THROWS_AS(title.style(), centui::invalid_element);
}

TEST_CASE("Single same-type container child nests while ordinary copies alias", "[document][container]")
{
    centui::container child;
    auto alias = child;
    REQUIRE(alias == child);
    centui::container<> root{child};
    REQUIRE(root != child);
    REQUIRE(child.valid());
    centui::document document{root};
    REQUIRE(root.id() != child.id());
    REQUIRE(alias.id() == child.id());
    REQUIRE(document.children(root).size() == 1);
    REQUIRE(document.children(root)[0] == alias);
    REQUIRE(document.parent(child).value() == root);
}

TEST_CASE("Identical styled containers retain every nested level", "[document][container]")
{
    struct panel_style
    {
        centui::length width = centui::px(120);
    };
    auto root = centui::container<panel_style>{
        centui::container<panel_style>{
            centui::container<panel_style>{
                centui::text{"Deep leaf"},
            },
        },
    };
    centui::document document{root};
    auto parent = document.root();
    for(int depth = 0; depth < 2; ++depth)
    {
        const auto children = document.children(parent);
        REQUIRE(children.size() == 1);
        const auto child = children.front();
        REQUIRE(child.kind() == centui::element_kind::container);
        REQUIRE(child.id() != parent.id());
        REQUIRE(document.parent(child).value() == parent);
        REQUIRE(child.style().width == centui::px(120));
        parent = child;
    }
    const auto leaves = document.children(parent);
    REQUIRE(leaves.size() == 1);
    REQUIRE(leaves[0].as<centui::text<>>().value() == "Deep leaf");
    REQUIRE(document.parent(leaves[0]).value() == parent);
}

TEST_CASE("Deduced single-child construction nests and preserves typed handles", "[document][container]")
{
    centui::text title{"Before"};
    centui::container child{title};
    auto root = centui::container{child};
    centui::document document{root};
    REQUIRE(root.id() != child.id());
    REQUIRE(document.children(root).size() == 1);
    REQUIRE(document.children(root)[0] == child);
    REQUIRE(document.parent(child).value() == root);
    title.value("After");
    REQUIRE(document.children(child)[0].as<centui::text<>>().value() == "After");
}

TEST_CASE("Brace construction supports empty containers and mixed styled children", "[document][container]")
{
    struct outer_style { centui::length width = centui::px(200); };
    struct inner_style { centui::length width = centui::px(100); };
    centui::container<> empty{};
    centui::container<> default_constructed;
    auto root = centui::container<outer_style>{
        centui::text{"First"},
        centui::container<inner_style>{centui::text{"Nested"}},
        empty,
        default_constructed,
        centui::button{"Action"},
    };
    centui::document document{root};
    const auto children = document.children(root);
    REQUIRE(children.size() == 5);
    REQUIRE(children[0].as<centui::text<>>().value() == "First");
    REQUIRE(children[1].style().width == centui::px(100));
    REQUIRE(document.children(children[1])[0].as<centui::text<>>().value() == "Nested");
    REQUIRE(document.children(empty).empty());
    REQUIRE(document.children(default_constructed).empty());
    REQUIRE(children[4].kind() == centui::element_kind::button);
}

TEST_CASE("Mounting temporaries and moving documents preserve handles", "[document]")
{
    centui::text title{"Child"};
    centui::document initial{centui::container<>{title}};
    const auto id = title.id();
    auto moved = std::move(initial);
    REQUIRE(title.id() == id);
    REQUIRE(moved.children(moved.root())[0] == title);
    REQUIRE_THROWS_AS(initial.empty(), std::logic_error);

    centui::text replaced{"Replaced"};
    centui::document destination{replaced};
    destination = std::move(moved);
    REQUIRE_FALSE(replaced.valid());
    REQUIRE(title.valid());
}

TEST_CASE("Moving a typed handle invalidates only its source", "[document]")
{
    centui::text title{"Value"};
    auto alias = title;
    auto moved = std::move(title);
    REQUIRE_FALSE(title.valid());
    REQUIRE_THROWS_AS(title.value(), centui::invalid_element);
    REQUIRE(moved == alias);
    centui::document document{moved};
    moved.value("Mounted");
    REQUIRE(alias.value() == "Mounted");
}

TEST_CASE("Growth reorder and reparent preserve node and widget identity", "[document]")
{
    centui::text title{"Stable"};
    centui::button action{"Action"};
    centui::container left{title, action};
    centui::container right;
    centui::container root{left, right};
    centui::document document{root};
    const auto id = title.id();
    for(int i = 0; i < 128; ++i) document.append(right, centui::text{std::to_string(i)});
    REQUIRE(title.id() == id);

    const std::array<centui::element, 2> order{action, title};
    document.reorder(left, order);
    REQUIRE(document.children(left)[1] == title);
    document.reparent(title, right, 0);
    REQUIRE(title.id() == id);
    REQUIRE(document.parent(title).value() == right);
    REQUIRE(document.children(right)[0] == title);
    REQUIRE(document.children(left).size() == 1);
    title.value("Still accessible");
    REQUIRE(document.children(right)[0].as<centui::text<>>().value() == "Still accessible");
    document.reparent(title, right, centui::document::end);
    REQUIRE(document.children(right).back() == title);
}

TEST_CASE("Invalid parenting and reorder operations leave topology intact", "[document]")
{
    centui::text title{"Child"};
    centui::button action{"Action"};
    centui::container branch{title, action};
    centui::container sibling;
    centui::container root{branch, sibling};
    centui::document document{root};
    centui::container foreign;
    centui::document other{foreign};

    REQUIRE_THROWS_AS(document.reparent(branch, branch), std::invalid_argument);
    REQUIRE_THROWS_AS(document.reparent(root, branch), std::invalid_argument);
    REQUIRE_THROWS_AS(document.reparent(title, foreign), std::invalid_argument);
    REQUIRE_THROWS_AS(document.append(root, foreign), std::invalid_argument);
    REQUIRE_THROWS_AS(document.append(root, title), std::invalid_argument);
    REQUIRE_THROWS_AS(document.append(title, centui::text{"Invalid"}), std::invalid_argument);
    REQUIRE_THROWS_AS(document.reparent(title, sibling, 2), std::out_of_range);
    const std::array<centui::element, 2> duplicate{title, title};
    REQUIRE_THROWS_AS(document.reorder(branch, duplicate), std::invalid_argument);
    REQUIRE(document.children(root).size() == 2);
    REQUIRE(document.children(branch)[0] == title);
    REQUIRE(document.children(branch)[1] == action);
    REQUIRE(document.children(sibling).empty());
    REQUIRE(other.root() == foreign);

    centui::container nested;
    document.append(branch, nested);
    REQUIRE_THROWS_AS(document.reparent(branch, nested), std::invalid_argument);
    REQUIRE(document.parent(nested).value() == branch);
}

TEST_CASE("Duplicate construction children fail without consuming named widgets", "[document]")
{
    centui::text title{"Unconsumed"};
    REQUIRE_THROWS_AS((centui::container<>{title, title}), std::invalid_argument);
    REQUIRE(title.valid());
    REQUIRE_FALSE(title.mounted());
    centui::container<> owner{title};
    REQUIRE_THROWS_AS((centui::container<>{title}), std::invalid_argument);
    centui::document document{owner};
    REQUIRE_THROWS_AS((centui::document{owner}), std::invalid_argument);
    REQUIRE(document.children(owner)[0] == title);
}

TEST_CASE("Duplicate same-type container children fail without consuming handles", "[document][container]")
{
    centui::container<> child;
    REQUIRE_THROWS_AS((centui::container<>{child, child}), std::invalid_argument);
    REQUIRE(child.valid());
    REQUIRE_FALSE(child.mounted());
    centui::container<> root{child};
    centui::document document{root};
    REQUIRE(document.children(root).size() == 1);
    REQUIRE(document.children(root)[0] == child);
}

TEST_CASE("Subtree removal invalidates descendants and root removal permits remount", "[document]")
{
    centui::text title{"Child"};
    centui::button action{"Action"};
    centui::container branch{title, action};
    centui::container sibling;
    centui::container root{branch, sibling};
    centui::document document{root};
    document.remove(branch);
    REQUIRE_FALSE(branch.valid());
    REQUIRE_FALSE(title.valid());
    REQUIRE_FALSE(action.valid());
    REQUIRE(sibling.valid());
    REQUIRE(document.children(root).size() == 1);
    document.remove(root);
    REQUIRE(document.empty());
    REQUIRE_FALSE(sibling.valid());
    REQUIRE_THROWS_AS(document.root(), std::logic_error);
    centui::text next{"New root"};
    document.mount(next);
    REQUIRE(document.root() == next);
}

TEST_CASE("Mounted preparation follows IDs after topology changes", "[document][layout][draw]")
{
    centui::font face;
    centui::text first{"First"};
    centui::text second{"Second"};
    centui::container root{first, second};
    root.patch_style({.padding = centui::edges::all(centui::px(12))});
    root.patch_style({.gap = centui::px(5)});
    centui::document document{root};
    {
        const auto commands = centui::prepare(document, face, 400, 200);
        REQUIRE(commands.size() == 2);
        REQUIRE(std::get<centui::text_command>(commands[0]).value == "First");
        REQUIRE(std::get<centui::text_command>(commands[0]).bounds.x == 12);
        REQUIRE(std::get<centui::text_command>(commands[1]).bounds.y > 12);
    }
    document.reparent(second, root, 0);
    first.value("Updated");
    {
        const auto commands = centui::prepare(document, face, 800, 300);
        REQUIRE(std::get<centui::text_command>(commands[0]).value == "Second");
        REQUIRE(std::get<centui::text_command>(commands[1]).value == "Updated");
    }
    document.remove(second);
    REQUIRE(centui::prepare(document, face, 400, 200).size() == 1);
}

TEST_CASE("Mounting a previously prepared construction tree retains its handles", "[document][layout]")
{
    centui::font face;
    centui::text title{"Prepared"};
    centui::container<> branch{title};
    centui::container<> root{branch};
    {
        const auto commands = centui::prepare(root, face, 400, 200);
        REQUIRE(commands.size() == 1);
    }
    centui::document document{root};
    title.value("Mounted");
    REQUIRE(document.parent(title).value() == branch);
    const auto commands = centui::prepare(document, face, 400, 200);
    REQUIRE(std::get<centui::text_command>(commands[0]).value == "Mounted");
}

namespace document_tests
{
struct backend
{
    centui::font face;
    centui::viewport viewport() const { return {400, 200}; }
    centui::font& glyphs() { return face; }
    void draw(std::span<const centui::draw_command>) {}
};
}

TEST_CASE("Document input survives reparenting and permits callback removal", "[document][input]")
{
    document_tests::backend backend;
    centui::ui ui{backend};
    int clicks = 0;
    centui::button action{"Action", {.on_click = [&] { ++clicks; }}};
    action.patch_style({.width = centui::px(100)});
    action.patch_style({.height = centui::px(40)});
    centui::container<> left{action};
    centui::container right;
    centui::container root{left, right};
    centui::document document{root};
    ui.process_event(document, {.type = centui::input_event_type::pointer_pressed, .x = 10, .y = 10});
    document.reparent(action, right);
    ui.process_event(document, {.type = centui::input_event_type::pointer_released, .x = 10, .y = 10});
    REQUIRE(clicks == 1);
    action.set_enabled(false);
    ui.process_event(document, {.type = centui::input_event_type::pointer_pressed, .x = 10, .y = 10});
    ui.process_event(document, {.type = centui::input_event_type::pointer_released, .x = 10, .y = 10});
    REQUIRE(clicks == 1);

    centui::button remove{"Remove", {.on_click = [&] { document.remove(root); }}};
    remove.patch_style({.width = centui::px(100)});
    remove.patch_style({.height = centui::px(40)});
    document.insert(root, remove, 0);
    ui.process_event(document, {.type = centui::input_event_type::pointer_pressed, .x = 10, .y = 10});
    ui.process_event(document, {.type = centui::input_event_type::pointer_released, .x = 10, .y = 10});
    REQUIRE(document.empty());
    REQUIRE_FALSE(remove.valid());
    ui.render(document);
}
