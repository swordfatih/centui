#include <catch2/catch_test_macros.hpp>

import std;
import centui.style;
import centui.node;
import centui.container;
import centui.text;

struct my_style
{
    int gap = 20;
    int padding = 12;
};

struct my_second_style
{
    int gap = 30;
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
    REQUIRE(container.node.style.gap == 20);
    REQUIRE(container.node.style.padding == 12);

    REQUIRE(container.node.children.size() == 2);

    REQUIRE(container.node.children[0].type == centui::node::type::text);
    REQUIRE(container.node.children[0].style.gap == 30);
    REQUIRE(container.node.children[0].style.padding == 12);

    REQUIRE(container.node.children[1].type == centui::node::type::text);
    REQUIRE(container.node.children[1].style.gap == 30);
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
    REQUIRE(root.node.style.gap == 20);
    REQUIRE(root.node.children.size() == 3);

    const auto& nested = root.node.children[1];

    REQUIRE(nested.type == centui::node::type::container);
    REQUIRE(nested.style.gap == 30);
    REQUIRE(nested.children.size() == 2);

    const auto& deep = nested.children[1];

    REQUIRE(deep.type == centui::node::type::container);
    REQUIRE(deep.style.gap == 20);
    REQUIRE(deep.style.padding == 12);
    REQUIRE(deep.children.size() == 1);
    REQUIRE(deep.children[0].type == centui::node::type::text);
}
