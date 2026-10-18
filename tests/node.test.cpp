#include <catch2/catch_test_macros.hpp>

import std;
import centui.style;
import centui.node;
import centui.layout;
import centui.container;
import centui.text;
import centui.unit;
import centui.font;
import centui.color;
import centui.draw;
import centui.ui;
import centui.input;
import centui.button;
import centui.glyph;

// Keep module-based tests in one translation unit for GCC 16's C++26 modules implementation.
#include "input.test.inc"

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

    REQUIRE(centui::details::element_access::get(container).style.gap == centui::length{20, centui::unit::px});
    REQUIRE(centui::details::element_access::get(container).style.padding == centui::edges::all(centui::px(12)));

    REQUIRE(centui::details::element_access::get(container).children.size() == 2);

    REQUIRE(std::holds_alternative<centui::text_data>(centui::details::element_access::get(container).children[0].data));
    REQUIRE(std::get<centui::text_data>(centui::details::element_access::get(container).children[0].data).value == "hello");
    REQUIRE(centui::details::element_access::get(container).children[0].style.gap == centui::length{30, centui::unit::px});
    REQUIRE(centui::details::element_access::get(container).children[0].style.padding == centui::edges::all(centui::px(12)));

    REQUIRE(std::holds_alternative<centui::text_data>(centui::details::element_access::get(container).children[1].data));
    REQUIRE(std::get<centui::text_data>(centui::details::element_access::get(container).children[1].data).value == "how are you");
    REQUIRE(centui::details::element_access::get(container).children[1].style.gap == centui::length{30, centui::unit::px});
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

    REQUIRE(std::holds_alternative<centui::container_data>(centui::details::element_access::get(root).data));
    REQUIRE(centui::details::element_access::get(root).style.gap == centui::length{20, centui::unit::px});
    REQUIRE(centui::details::element_access::get(root).children.size() == 3);

    const auto& nested = centui::details::element_access::get(root).children[1];

    REQUIRE(std::holds_alternative<centui::container_data>(nested.data));
    REQUIRE(nested.style.gap == centui::length{30, centui::unit::px});
    REQUIRE(nested.children.size() == 2);

    const auto& deep = nested.children[1];

    REQUIRE(std::holds_alternative<centui::container_data>(deep.data));
    REQUIRE(deep.style.gap == centui::length{20, centui::unit::px});
    REQUIRE(deep.style.padding == centui::edges::all(centui::px(12)));
    REQUIRE(deep.children.size() == 1);

    REQUIRE(std::holds_alternative<centui::text_data>(deep.children[0].data));
    REQUIRE(std::get<centui::text_data>(deep.children[0].data).value == "Deeply nested");
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

    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).layout.calculate(800, 600);

    const auto bounds = centui::details::element_access::get(root).layout.bounds();

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

    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).layout.calculate(800, 600);

    const auto first = centui::details::element_access::get(root).children[0].layout.bounds();
    const auto second = centui::details::element_access::get(root).children[1].layout.bounds();

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

    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).layout.calculate(800, 600);

    const auto child = centui::details::element_access::get(root).children[0].layout.bounds();
    const auto grandchild = centui::details::element_access::get(root).children[0].children[0].layout.bounds();

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

    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).layout.calculate(800, 600);

    REQUIRE(centui::details::element_access::get(root).layout.bounds().width == 100);

    centui::details::element_access::get(root).style.width = centui::px(200);
    centui::details::element_access::get(root).update();
    centui::details::element_access::get(root).layout.calculate(800, 600);

    REQUIRE(centui::details::element_access::get(root).layout.bounds().width == 200);
    REQUIRE(centui::details::element_access::get(root).layout.bounds().height == 50);
}

TEST_CASE("Calculate text layout", "[node][layout]")
{
    centui::font face;

    centui::container<> root{
        centui::text<>{"Hello CentUI"}};

    centui::details::element_access::get(root).attach(face);
    centui::details::element_access::get(root).calculate(800, 600);

    const auto bounds = centui::details::element_access::get(root).children[0].layout.bounds();

    REQUIRE(bounds.width > 0);
    REQUIRE(bounds.height > 0);
}

struct test_panel_style
{
    centui::length width = centui::px(200);
    centui::length height = centui::px(100);
    struct
    {
        centui::color normal = centui::color::from_hex(0x336699);
    } background_color;
};

TEST_CASE("Generate rectangle draw commands", "[draw]")
{
    centui::container<test_panel_style> root;

    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).calculate(800, 600);

    const auto result = centui::commands(centui::details::element_access::get(root));

    REQUIRE(result.size() == 1);
    REQUIRE(std::holds_alternative<centui::rectangle_command>(result[0]));

    const auto& rectangle = std::get<centui::rectangle_command>(result[0]);

    REQUIRE(rectangle.bounds.x == 0);
    REQUIRE(rectangle.bounds.y == 0);
    REQUIRE(rectangle.bounds.width == 200);
    REQUIRE(rectangle.bounds.height == 100);
    REQUIRE(rectangle.color == centui::color::from_hex(0x336699));
}

TEST_CASE("Generate nested rectangle draw commands", "[draw]")
{
    struct parent_style
    {
        centui::length width = centui::px(300);
        centui::length height = centui::px(200);
        centui::edges  padding = centui::edges::all(centui::px(20));
        struct
        {
            centui::color normal = centui::color::from_hex(0x222222);
        } background_color;
    };

    centui::container<parent_style> root{
        centui::container<test_panel_style>{}};

    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).calculate(800, 600);

    const auto result = centui::commands(centui::details::element_access::get(root));

    REQUIRE(result.size() == 2);
    REQUIRE(std::holds_alternative<centui::rectangle_command>(result[0]));
    REQUIRE(std::holds_alternative<centui::rectangle_command>(result[1]));

    const auto& parent = std::get<centui::rectangle_command>(result[0]);
    const auto& child = std::get<centui::rectangle_command>(result[1]);

    REQUIRE(parent.bounds.x == 0);
    REQUIRE(parent.bounds.y == 0);
    REQUIRE(parent.bounds.width == 300);
    REQUIRE(parent.bounds.height == 200);
    REQUIRE(parent.color == centui::color::from_hex(0x222222));

    REQUIRE(child.bounds.x == 20);
    REQUIRE(child.bounds.y == 20);
    REQUIRE(child.bounds.width == 200);
    REQUIRE(child.bounds.height == 100);
    REQUIRE(child.color == centui::color::from_hex(0x336699));
}

TEST_CASE("Generate text draw commands", "[draw]")
{
    centui::font   face;
    centui::text<> label{"Hello"};

    centui::details::element_access::get(label).attach(face);
    centui::details::element_access::get(label).calculate(800, 600);

    const auto result = centui::commands(centui::details::element_access::get(label));

    REQUIRE(result.size() == 1);
    REQUIRE(std::holds_alternative<centui::text_command>(result[0]));

    const auto& text = std::get<centui::text_command>(result[0]);

    REQUIRE(text.value == "Hello");
    REQUIRE(text.font_size == 16.0f);
    REQUIRE(text.bounds.width > 0);
    REQUIRE(text.bounds.height > 0);
}

TEST_CASE("Text updates refresh measurement and borrow the node layout", "[node][layout][draw]")
{
    centui::font face;
    centui::text label{"Hi"};
    centui::details::element_access::get(label).attach(face);
    const auto old_width = std::get<centui::text_data>(centui::details::element_access::get(label).data).shaped->bounds.width;

    std::get<centui::text_data>(centui::details::element_access::get(label).data).value = "Hello updated text";
    centui::details::element_access::get(label).style.font_size = 32;
    centui::details::element_access::get(label).update(face);
    centui::details::element_access::get(label).calculate(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());

    const auto& data = std::get<centui::text_data>(centui::details::element_access::get(label).data);
    const auto result = centui::commands(centui::details::element_access::get(label));
    const auto& command = std::get<centui::text_command>(result[0]);
    REQUIRE(command.shaped.has_value());
    REQUIRE(std::addressof(*command.shaped) == std::addressof(*data.shaped));
    REQUIRE(command.value.data() == data.value.data());
    REQUIRE(command.shaped->bounds.width == face.measure(data.value, 32).width);
    REQUIRE(command.shaped->bounds.width > old_width);
    REQUIRE(command.font_size == 32);
    REQUIRE(command.bounds.width == std::ceil(command.shaped->bounds.width));
    REQUIRE(command.bounds.height == std::ceil(command.shaped->bounds.height));
}

TEST_CASE("Explicit synchronization updates child membership and order", "[node][layout]")
{
    centui::container root{centui::container{}, centui::container{}};
    centui::details::element_access::get(root).children[0].style.height = centui::px(10);
    centui::details::element_access::get(root).children[1].style.height = centui::px(20);
    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).calculate(100, 100);
    REQUIRE(centui::details::element_access::get(root).children[1].layout.bounds().y == 10);

    std::swap(centui::details::element_access::get(root).children[0], centui::details::element_access::get(root).children[1]);
    centui::details::element_access::get(root).children.emplace_back();
    centui::details::element_access::get(root).children.back().style.height = centui::px(30);
    centui::details::element_access::get(root).sync_children();
    centui::details::element_access::get(root).calculate(100, 100);
    REQUIRE(centui::details::element_access::get(root).children[1].layout.bounds().y == 20);
    REQUIRE(centui::details::element_access::get(root).children[2].layout.bounds().y == 30);

    centui::details::element_access::get(root).children.erase(centui::details::element_access::get(root).children.begin());
    centui::details::element_access::get(root).sync_children();
    centui::details::element_access::get(root).calculate(100, 100);
    REQUIRE(centui::details::element_access::get(root).children[0].layout.bounds().y == 0);
    REQUIRE(centui::details::element_access::get(root).children[1].layout.bounds().y == 10);
}

TEST_CASE("Measured nodes survive moves and font destruction during calculation", "[node][layout]")
{
    centui::container root{centui::text{"Hello"}};
    float expected_height = 0;
    {
        centui::font face;
        expected_height = face.measure("Hello", 16).height;
        centui::details::element_access::get(root).attach(face);
    }

    auto moved = std::move(centui::details::element_access::get(root));
    moved.calculate(800, 600);
    REQUIRE(moved.children[0].layout.bounds().height == std::round(expected_height));

    // Explicit attachment without a font clears text measurement.
    moved.attach();
    moved.calculate(800, 600);
    REQUIRE(moved.children[0].layout.bounds().height == 0);
}

TEST_CASE("Changing text to container clears its measurement", "[node][layout]")
{
    centui::font face;
    centui::text label{"Hello"};
    centui::details::element_access::get(label).attach(face);
    centui::details::element_access::get(label).data = centui::container_data{};
    centui::details::element_access::get(label).children.emplace_back();
    centui::details::element_access::get(label).children[0].style.height = centui::px(45);
    centui::details::element_access::get(label).sync_children(face);
    centui::details::element_access::get(label).calculate(100, 100);
    REQUIRE(centui::details::element_access::get(label).children[0].layout.bounds().height == 45);

    centui::details::element_access::get(label).data = centui::text_data{"Invalid"};
    REQUIRE_THROWS_AS(centui::details::element_access::get(label).update(), std::invalid_argument);
}

TEST_CASE("Borrowed draw commands preserve background and sibling order", "[draw]")
{
    centui::font face;
    centui::container<test_panel_style> root{centui::text{"First"}, centui::text{"Last"}};
    centui::details::element_access::get(root).children[0].style.background_color.normal = centui::color::from_hex(0xFF0000);
    centui::details::element_access::get(root).attach(face);
    centui::details::element_access::get(root).calculate(800, 600);
    const auto result = centui::commands(centui::details::element_access::get(root));

    REQUIRE(result.size() == 4);
    REQUIRE(std::holds_alternative<centui::rectangle_command>(result[0]));
    REQUIRE(std::holds_alternative<centui::rectangle_command>(result[1]));
    REQUIRE(std::get<centui::text_command>(result[2]).value == "First");
    REQUIRE(std::get<centui::text_command>(result[3]).value == "Last");
    REQUIRE_FALSE(std::get<centui::text_command>(result[2]).shaped->glyphs.empty());
}

TEST_CASE("Style updates preserve child links and existing shaping", "[node][layout]")
{
    centui::font face;
    centui::container root{centui::text{"First"}, centui::text{"Last"}};
    centui::details::element_access::get(root).attach(face);
    centui::details::element_access::get(root).calculate(800, 600);
    const auto old_y = centui::details::element_access::get(root).children[1].layout.bounds().y;
    const auto* glyphs = std::get<centui::text_data>(centui::details::element_access::get(root).children[0].data).shaped->glyphs.data();

    centui::details::element_access::get(root).style.background_color.normal = centui::color::from_hex(0x123456);
    centui::details::element_access::get(root).update();
    // Removing and reinserting children would reset these calculated bounds.
    REQUIRE(centui::details::element_access::get(root).children[1].layout.bounds().y == old_y);
    REQUIRE(std::get<centui::text_data>(centui::details::element_access::get(root).children[0].data).shaped->glyphs.data() == glyphs);
    centui::details::element_access::get(root).calculate(800, 600);
    REQUIRE(centui::details::element_access::get(root).children[1].layout.bounds().y == old_y);

    centui::details::element_access::get(root).style.gap = centui::px(10);
    centui::details::element_access::get(root).update();
    centui::details::element_access::get(root).calculate(800, 600);
    REQUIRE(centui::details::element_access::get(root).children[1].layout.bounds().y == old_y + 10);
}

TEST_CASE("Explicit font updates work after the original font is destroyed", "[node][layout]")
{
    centui::text label{"First"};
    {
        centui::font original;
        centui::details::element_access::get(label).attach(original);
    }
    centui::font replacement;
    std::get<centui::text_data>(centui::details::element_access::get(label).data).value = "Replacement";
    centui::details::element_access::get(label).update(replacement);
    centui::details::element_access::get(label).calculate(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
    REQUIRE(centui::details::element_access::get(label).layout.bounds().width == std::ceil(replacement.measure("Replacement", 16).width));
}

TEST_CASE("Layout enum translation preserves row alignment and absolute position", "[layout]")
{
    centui::container root;
    centui::details::element_access::get(root).children.emplace_back();
    centui::details::element_access::get(root).style.width = centui::px(200);
    centui::details::element_access::get(root).style.height = centui::px(100);
    centui::details::element_access::get(root).style.flex_direction = centui::direction::row;
    centui::details::element_access::get(root).style.align_items = centui::alignment::end;
    centui::details::element_access::get(root).style.justify_content = centui::justification::center;
    auto& child = centui::details::element_access::get(root).children[0];
    child.style.width = centui::px(40);
    child.style.height = centui::px(20);
    centui::details::element_access::get(root).attach();
    centui::details::element_access::get(root).calculate(200, 100);
    REQUIRE(child.layout.bounds().x == 80);
    REQUIRE(child.layout.bounds().y == 80);

    child.style.position = centui::position::absolute;
    child.style.inset.left = centui::px(7);
    child.style.inset.top = centui::px(9);
    centui::details::element_access::get(root).update();
    centui::details::element_access::get(root).calculate(200, 100);
    REQUIRE(child.layout.bounds().x == 7);
    REQUIRE(child.layout.bounds().y == 9);

    child.style.position = static_cast<centui::position>(100);
    REQUIRE_THROWS_AS(centui::details::element_access::get(root).update(), std::invalid_argument);
}

static_assert(std::is_move_constructible_v<centui::text<>>);
static_assert(std::is_move_constructible_v<centui::container<>>);
static_assert(!std::is_copy_constructible_v<centui::container<>>);

template <typename type>
concept exposes_node = requires(type& value)
{
    value.node;
};

static_assert(!exposes_node<centui::container<>>);
static_assert(!exposes_node<centui::text<>>);

TEST_CASE("Public elements prepare without windows or graphics initialization", "[ui][headless]")
{
    centui::font face;
    centui::container root{centui::text{"First"}, centui::text{"Last"}};
    root.style().background_color.normal = centui::color::from_hex(0x123456);
    root.style().padding = centui::edges::all(centui::px(12));

    const auto result = centui::prepare(root, face, 400, 200);
    REQUIRE(result.size() == 3);
    REQUIRE(std::get<centui::rectangle_command>(result[0]).bounds.width == 400);
    REQUIRE(std::get<centui::text_command>(result[1]).value == "First");
    REQUIRE(std::get<centui::text_command>(result[1]).bounds.x == 12);
    REQUIRE(std::get<centui::text_command>(result[2]).value == "Last");
}

TEST_CASE("Public preparation reflects viewport resizing", "[ui][headless]")
{
    centui::font face;
    centui::viewport viewport{400, 200};
    centui::container root;
    root.style().width = centui::percent(50);
    root.style().height = centui::percent(50);
    root.style().background_color.normal = centui::color::from_hex(0x123456);
    {
        const auto result = centui::prepare(root, face, viewport.width, viewport.height);
        REQUIRE(std::get<centui::rectangle_command>(result[0]).bounds.width == 200);
        REQUIRE(std::get<centui::rectangle_command>(result[0]).bounds.height == 100);
    }
    viewport = {800, 600};
    {
        const auto result = centui::prepare(root, face, viewport.width, viewport.height);
        REQUIRE(std::get<centui::rectangle_command>(result[0]).bounds.width == 400);
        REQUIRE(std::get<centui::rectangle_command>(result[0]).bounds.height == 300);
    }
    REQUIRE(centui::prepare(root, face, 0, 0).empty());
    REQUIRE(centui::prepare(root, face, -1, 100).empty());
}

TEST_CASE("Moved public elements prepare safely after font destruction", "[ui][headless]")
{
    centui::text label{"Before"};
    {
        centui::font initial;
        REQUIRE(centui::prepare(label, initial, 400, 200).size() == 1);
    }
    auto moved = std::move(label);
    moved.value("After moving");
    moved.style().font_size = 24;
    centui::font face;
    {
        const auto result = centui::prepare(moved, face, 400, 200);
        const auto& command = std::get<centui::text_command>(result[0]);
        REQUIRE(command.value == "After moving");
        REQUIRE(command.font_size == 24);
        REQUIRE(command.shaped->bounds.width == face.measure("After moving", 24).width);
    }
    centui::container root{std::move(moved)};
    {
        const auto result = centui::prepare(root, face, 400, 200);
        REQUIRE(result.size() == 1);
        REQUIRE(std::get<centui::text_command>(result[0]).value == "After moving");
    }
    auto moved_root = std::move(root);
    REQUIRE(centui::prepare(moved_root, face, 800, 600).size() == 1);
}

TEST_CASE("Public preparation refreshes repeated text and style edits", "[ui][headless]")
{
    centui::font face;
    centui::text label{"First"};
    for(int i = 0; i < 4; ++i)
    {
        label.value(std::string(i + 1, 'W'));
        label.style().font_size = 16.0f + i;
        const auto result = centui::prepare(label, face, 400, 200);
        const auto& command = std::get<centui::text_command>(result[0]);
        REQUIRE(command.value == label.value());
        REQUIRE(command.shaped->bounds.width == face.measure(label.value(), label.style().font_size).width);
    }
}

struct recording_backend
{
    centui::font face;
    centui::viewport size{400, 200};
    int submissions = 0;
    float drawn_width = 0;
    std::string drawn_text;

    const centui::viewport& viewport() const
    {
        return size;
    }

    centui::font& glyphs()
    {
        return face;
    }

    void draw(std::span<const centui::draw_command> commands)
    {
        ++submissions;
        for(const auto& command: commands)
        {
            if(const auto* rectangle = std::get_if<centui::rectangle_command>(&command))
            {
                drawn_width = rectangle->bounds.width;
            }
            if(const auto* text = std::get_if<centui::text_command>(&command))
            {
                drawn_text = text->value;
            }
        }
    }
};

TEST_CASE("UI borrows a concrete backend without any window or graphics API", "[ui][headless]")
{
    recording_backend backend;
    centui::ui ui{backend};
    centui::container root{centui::text{"HUD"}};
    root.style().background_color.normal = centui::color::from_hex(0x123456);
    ui.render(root);
    REQUIRE(backend.submissions == 1);
    REQUIRE(backend.drawn_text == "HUD");
    REQUIRE(backend.drawn_width == 400);
    backend.size.width = 800;
    auto moved = std::move(root);
    ui.render(moved);
    REQUIRE(backend.submissions == 2);
    REQUIRE(backend.drawn_width == 800);
    backend.size = {};
    ui.render(moved);
    REQUIRE(backend.submissions == 2);
}

TEST_CASE("Optional draw layout borrows the prepared value or stays empty", "[draw]")
{
    centui::text label{"Hello"};
    auto& tree = centui::details::element_access::get(label);
    tree.attach();
    tree.calculate(400, 200);
    {
        const auto result = centui::commands(tree);
        REQUIRE_FALSE(std::get<centui::text_command>(result[0]).shaped.has_value());
    }
    centui::font face;
    {
        const auto result = centui::prepare(label, face, 400, 200);
        const auto& command = std::get<centui::text_command>(result[0]);
        REQUIRE(command.shaped.has_value());
        REQUIRE(std::addressof(command.shaped.value()) == std::addressof(std::get<centui::text_data>(tree.data).shaped.value()));
    }
}

TEST_CASE("Measurement callbacks can own move-only state and survive layout moves", "[layout]")
{
    centui::layout layout;
    layout.update(centui::style{});
    layout.set_measure([height = std::make_unique<float>(25.0f)](float, float)
    {
        return centui::rect{0, 0, 40, *height};
    });
    auto moved = std::move(layout);
    moved.calculate(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
    REQUIRE(moved.bounds().width == 40);
    REQUIRE(moved.bounds().height == 25);
    moved.set_measure({});
    moved.calculate(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
    REQUIRE(moved.bounds().height == 0);
}

TEST_CASE("Fit padding and gap reset previous explicit values", "[layout]")
{
    centui::container root{centui::container{}, centui::container{}};
    auto& tree = centui::details::element_access::get(root);
    tree.children[0].style.height = centui::px(10);
    tree.children[1].style.height = centui::px(20);
    root.style().padding = centui::edges::all(centui::px(5));
    root.style().gap = centui::px(7);
    tree.attach();
    tree.calculate(100, 100);
    REQUIRE(tree.children[1].layout.bounds().y == 22);
    root.style().padding = centui::edges::all(centui::fit());
    root.style().gap = centui::fit();
    tree.update();
    tree.calculate(100, 100);
    REQUIRE(tree.children[1].layout.bounds().y == 10);
}
