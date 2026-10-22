#include <array>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

import centui.tree;
import centui.widgets;

namespace
{

class custom_leaf final : public centui::widget
{
public:
    explicit custom_leaf(int value = 0, int* destroyed = nullptr)
        : _value(value), _destroyed(destroyed)
    {
    }

    ~custom_leaf() override
    {
        if(_destroyed)
        {
            ++*_destroyed;
        }
    }

    [[nodiscard]] int value() const noexcept
    {
        return _value;
    }

    void set_value(int value)
    {
        if(value != _value)
        {
            invalidate_layout();
            _value = value;
        }
    }

protected:
    centui::size measure(const centui::measure_context&) override
    {
        return {static_cast<float>(_value), 12};
    }

    void paint(centui::painter& output) const override
    {
        output.rectangle(bounds(), centui::color::from_hex(0x123456));
    }

private:
    int _value;
    int* _destroyed;
};

class custom_composite final : public centui::widget
{
public:
    explicit custom_composite(std::span<const centui::handle<>> children = {})
    {
        append(children);
    }

    void compose(std::span<const centui::handle<>> children)
    {
        append(children);
    }
};

}

TEST_CASE("Typed handles share identity and expose derived setters", "[widget][handles]")
{
    auto original = centui::make_widget<custom_leaf>(7);
    auto alias = original;
    centui::handle<> erased = original;
    REQUIRE(&original.get() == &alias.get());
    REQUIRE(&erased.get() == &original.get());
    alias->set_value(19);
    REQUIRE(original->value() == 19);

    centui::text<> label{"Before"};
    auto captured = label;
    auto edit = [captured]() mutable
    {
        captured.set_text("After");
    };
    edit();
    REQUIRE(label.value() == "After");
}

TEST_CASE("Empty and moved handles reject access without disturbing aliases", "[widget][handles]")
{
    centui::handle<custom_leaf> empty;
    REQUIRE_FALSE(empty.valid());
    REQUIRE_FALSE(empty.weak().lock().valid());
    REQUIRE_THROWS_AS(empty.get(), std::logic_error);
    REQUIRE_THROWS_AS(empty->value(), std::logic_error);

    auto source = centui::make_widget<custom_leaf>(11);
    auto alias = source;
    auto moved = std::move(source);
    REQUIRE_FALSE(source.valid());
    REQUIRE_THROWS_AS(source.get(), std::logic_error);
    REQUIRE(moved == alias);
    empty = std::move(moved);
    REQUIRE_FALSE(moved.valid());
    REQUIRE(empty == alias);
    REQUIRE(empty->value() == 11);

    centui::text<> title{"Moved"};
    auto destination = std::move(title);
    REQUIRE_FALSE(title.valid());
    REQUIRE_THROWS_AS(title.value(), std::logic_error);
    REQUIRE(destination.value() == "Moved");
}

TEST_CASE("Weak handles expire after the last shared owner", "[widget][lifetime]")
{
    int destroyed = 0;
    centui::weak_handle<custom_leaf> weak;
    {
        auto leaf = centui::make_widget<custom_leaf>(3, &destroyed);
        weak = leaf.weak();
        auto locked = weak.lock();
        leaf = {};
        REQUIRE(locked.valid());
        REQUIRE(weak.lock().valid());
        REQUIRE(destroyed == 0);
    }
    REQUIRE_FALSE(weak.lock().valid());
    REQUIRE(destroyed == 1);
}

TEST_CASE("Parent destruction releases children without invalidating surviving handles", "[widget][lifetime]")
{
    int destroyed = 0;
    auto child = centui::make_widget<custom_leaf>(5, &destroyed);
    auto weak_child = child.weak();
    centui::weak_handle<custom_composite> weak_parent;
    {
        const std::array<centui::handle<>, 1> children{child};
        auto parent = centui::make_widget<custom_composite>(std::span{children});
        weak_parent = parent.weak();
        child = {};
        REQUIRE(weak_child.lock().valid());
        child = weak_child.lock();
    }
    REQUIRE_FALSE(weak_parent.lock().valid());
    REQUIRE(child.valid());
    REQUIRE(destroyed == 0);
    {
        const std::array<centui::handle<>, 1> children{child};
        auto replacement = centui::make_widget<custom_composite>(std::span{children});
        REQUIRE(replacement.valid());
    }
    child = {};
    REQUIRE_FALSE(weak_child.lock().valid());
    REQUIRE(destroyed == 1);
}

TEST_CASE("Duplicate and parented construction batches fail atomically", "[widget][tree]")
{
    auto free_child = centui::make_widget<custom_leaf>(1);
    const std::array<centui::handle<>, 2> duplicate{free_child, free_child};
    REQUIRE_THROWS_AS(centui::make_widget<custom_composite>(std::span{duplicate}), std::invalid_argument);

    auto owned_child = centui::make_widget<custom_leaf>(2);
    const std::array<centui::handle<>, 1> owned{owned_child};
    auto owner = centui::make_widget<custom_composite>(std::span{owned});
    const std::array<centui::handle<>, 2> mixed{free_child, owned_child};
    REQUIRE_THROWS_AS(centui::make_widget<custom_composite>(std::span{mixed}), std::invalid_argument);

    const std::array<centui::handle<>, 1> detached{free_child};
    auto accepted = centui::make_widget<custom_composite>(std::span{detached});
    centui::container<> root{owner, accepted};
    centui::document document{root};
    REQUIRE(document.children(owner).front() == centui::handle<>{owned_child});
    REQUIRE(document.children(accepted).front() == centui::handle<>{free_child});
}

TEST_CASE("Composite extension rejects self attachment and ancestor cycles", "[widget][tree]")
{
    auto child = centui::make_widget<custom_composite>();
    const std::array<centui::handle<>, 1> children{child};
    auto parent = centui::make_widget<custom_composite>(std::span{children});
    const std::array<centui::handle<>, 1> ancestor{parent};
    const std::array<centui::handle<>, 1> self{child};
    REQUIRE_THROWS_AS(child->compose(std::span{ancestor}), std::invalid_argument);
    REQUIRE_THROWS_AS(child->compose(std::span{self}), std::invalid_argument);

    centui::document document{parent};
    REQUIRE(document.children(parent).size() == 1);
    REQUIRE(document.parent(child) == centui::handle<>{parent});
    REQUIRE(document.children(child).empty());
}

TEST_CASE("A single same-type container is nested while ordinary copies alias", "[widget][container]")
{
    centui::container<> child;
    auto alias = child;
    centui::container<> parent{child};
    REQUIRE(alias == child);
    REQUIRE_FALSE(parent == child);

    centui::document document{parent};
    REQUIRE(document.children(parent).size() == 1);
    REQUIRE(document.children(parent).front() == centui::handle<>{alias});
    REQUIRE(document.parent(child) == centui::handle<>{parent});
}

TEST_CASE("Nested temporary containers preserve every level and typed leaf", "[widget][container]")
{
    auto leaf = centui::make_widget<custom_leaf>(29);
    auto root = centui::container<>{centui::container<>{centui::container<>{leaf}}};
    centui::document document{root};
    auto current = document.root();
    for(int depth = 0; depth < 2; ++depth)
    {
        auto children = document.children(current);
        REQUIRE(children.size() == 1);
        REQUIRE_FALSE(children.front() == current);
        REQUIRE(document.parent(children.front()) == current);
        current = children.front();
    }
    REQUIRE(document.children(current).size() == 1);
    REQUIRE(document.children(current).front() == centui::handle<>{leaf});
    leaf->set_value(31);
    REQUIRE(leaf->value() == 31);
}

TEST_CASE("Document destruction detaches a surviving custom subtree", "[widget][lifetime]")
{
    auto leaf = centui::make_widget<custom_leaf>(13);
    const std::array<centui::handle<>, 1> children{leaf};
    auto root = centui::make_widget<custom_composite>(std::span{children});
    {
        centui::document document{root};
        REQUIRE(document.children(root).front() == centui::handle<>{leaf});
    }
    REQUIRE(root.valid());
    REQUIRE(leaf.valid());
    leaf->set_value(17);
    centui::document remounted{root};
    REQUIRE(remounted.children(root).front() == centui::handle<>{leaf});
    REQUIRE(leaf->value() == 17);
}

TEST_CASE("Custom measurement painting and invalidation use the public extension hooks", "[widget][extension]")
{
    auto leaf = centui::make_widget<custom_leaf>(29);
    centui::container<> root{leaf};
    auto appearance = root.style();
    appearance.width = centui::px(200);
    appearance.height = centui::px(100);
    appearance.align_items = centui::alignment::start;
    root.set_style(appearance);
    centui::document document{root};

    REQUIRE(document.prepare({200, 100}).commands.size() == 1);
    REQUIRE(leaf->bounds().width == 29);
    REQUIRE(leaf->bounds().height == 12);
    leaf->set_value(47);
    REQUIRE(document.prepare({200, 100}).commands.size() == 1);
    REQUIRE(leaf->bounds().width == 47);
}
