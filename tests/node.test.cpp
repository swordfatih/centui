#include <catch2/catch_test_macros.hpp>

import std;
import centui.style;
import centui.node;
import centui.container;
import centui.text;
import centui.unit;

struct my_style
{
    centui::length gap = centui::px(20);
    centui::edges  padding = centui::edges::all(centui::px(12));
};

struct my_second_style
{
    centui::length gap = centui::px(30);
};

TEST_CASE("Append children to a node", "[node]")
{
    centui::node root;

    root.children.emplace_back();
    root.children.emplace_back();

    REQUIRE(root.children.size() == 2);
}

TEST_CASE("Create a container node", "[node]")
{
    centui::container<my_style> container{
        centui::text<my_style, my_second_style>{"hello"},
        centui::text<my_second_style>{"how are you"},
    };

    REQUIRE(container.node.type == centui::node::type::container);
    REQUIRE(container.node.style.gap == centui::length{20, centui::unit::px});
    REQUIRE(container.node.style.padding == centui::edges::all(centui::px(12)));

    REQUIRE(container.node.children.size() == 2);

    REQUIRE(container.node.children[0].type == centui::node::type::text);
    REQUIRE(container.node.children[0].style.gap == centui::length{30, centui::unit::px});
    REQUIRE(container.node.children[0].style.padding == centui::edges::all(centui::px(12)));

    REQUIRE(container.node.children[1].type == centui::node::type::text);
    REQUIRE(container.node.children[1].style.gap == centui::length{30, centui::unit::px});
}

TEST_CASE("Create nested container nodes", "[node]")
{
    centui::container<my_style> root{
        centui::text{"First"},
        centui::container<my_second_style>{
            centui::text{"Nested first"},
            centui::container<my_style>{
                centui::text{"Deeply nested"},
            },
        },
        centui::text{"Last"},
    };

    REQUIRE(root.node.type == centui::node::type::container);
    REQUIRE(root.node.style.gap == centui::length{20, centui::unit::px});
    REQUIRE(root.node.children.size() == 3);

    const auto& nested = root.node.children[1];

    REQUIRE(nested.type == centui::node::type::container);
    REQUIRE(nested.style.gap == centui::length{30, centui::unit::px});
    REQUIRE(nested.children.size() == 2);

    const auto& deep = nested.children[1];

    REQUIRE(deep.type == centui::node::type::container);
    REQUIRE(deep.style.gap == centui::length{20, centui::unit::px});
    REQUIRE(deep.style.padding == centui::edges::all(centui::px(12)));
    REQUIRE(deep.children.size() == 1);
    REQUIRE(deep.children[0].type == centui::node::type::text);
}

TEST_CASE("Calculate container layout", "[node][layout]")
{
    struct fixed_style
    {
        centui::length width = centui::px(300);
        centui::length height = centui::px(200);
        centui::edges  padding = centui::edges::all(centui::px(20));
    };

    centui::container<fixed_style> root;

    root.node.attach();
    root.node.layout.calculate(800, 600);

    const auto bounds = root.node.layout.bounds();

    REQUIRE(bounds.width == 300);
    REQUIRE(bounds.height == 200);
}

TEST_CASE("Calculate child positions with gap and padding", "[node][layout]")
{
    struct parent_style
    {
        centui::length width = centui::px(300);
        centui::length height = centui::px(200);
        centui::length gap = centui::px(10);
        centui::edges  padding = centui::edges::all(centui::px(20));
    };

    struct child_style
    {
        centui::length width = centui::px(100);
        centui::length height = centui::px(30);
    };

    centui::container<parent_style> root{
        centui::container<child_style>{},
        centui::container<child_style>{},
    };

    root.node.attach();
    root.node.layout.calculate(800, 600);

    const auto first = root.node.children[0].layout.bounds();
    const auto second = root.node.children[1].layout.bounds();

    REQUIRE(first.x == 20);
    REQUIRE(first.y == 20);
    REQUIRE(first.width == 100);
    REQUIRE(first.height == 30);

    REQUIRE(second.x == 20);
    REQUIRE(second.y == 60);
    REQUIRE(second.width == 100);
    REQUIRE(second.height == 30);
}

TEST_CASE("Calculate nested container layout", "[node][layout]")
{
    struct parent_style
    {
        centui::length width = centui::px(300);
        centui::length height = centui::px(200);
        centui::edges  padding = centui::edges::all(centui::px(20));
    };

    struct child_style
    {
        centui::length width = centui::px(100);
        centui::length height = centui::px(80);
        centui::edges  padding = centui::edges::all(centui::px(10));
    };

    struct grandchild_style
    {
        centui::length width = centui::px(40);
        centui::length height = centui::px(20);
    };

    centui::container<parent_style> root{
        centui::container<child_style>{
            centui::container<grandchild_style>{},
        },
    };

    root.node.attach();
    root.node.layout.calculate(800, 600);

    const auto child = root.node.children[0].layout.bounds();
    const auto grandchild = root.node.children[0].children[0].layout.bounds();

    REQUIRE(child.x == 20);
    REQUIRE(child.y == 20);
    REQUIRE(child.width == 100);
    REQUIRE(child.height == 80);

    REQUIRE(grandchild.x == 10);
    REQUIRE(grandchild.y == 10);
    REQUIRE(grandchild.width == 40);
    REQUIRE(grandchild.height == 20);
}

TEST_CASE("Update node layout", "[node][layout]")
{
    struct fixed_style
    {
        centui::length width = centui::px(100);
        centui::length height = centui::px(50);
    };

    centui::container<fixed_style> root;

    root.node.attach();
    root.node.layout.calculate(800, 600);

    REQUIRE(root.node.layout.bounds().width == 100);

    root.node.style.width = centui::px(200);
    root.node.update();
    root.node.layout.calculate(800, 600);

    REQUIRE(root.node.layout.bounds().width == 200);
    REQUIRE(root.node.layout.bounds().height == 50);
}