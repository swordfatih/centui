#include <stdexcept>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct base_theme
{
    float font_size = 16;
    centui::length width = centui::px(40);
};

struct secondary_theme
{
    float font_size = 14;
};

class activation_probe : public centui::pressable_widget
{
public:
    using pressable_widget::activate;
    using pressable_widget::pressable_widget;
};

} // namespace

TEST_CASE("Document patches retain composition order and persistent local overrides", "[style][document]")
{
    auto base = centui::container<base_theme>{};
    auto secondary = centui::container<base_theme, secondary_theme>{};
    auto root = centui::container{base, secondary};
    centui::document document{root};
    document.set_style({.font_size = 12});
    document.patch_style<base_theme>({.font_size = 20});
    REQUIRE(base.style().font_size == 20);
    REQUIRE(secondary.style().font_size == 14);
    REQUIRE(root.style().font_size == 12);

    base.patch_style({.font_size = 20}); // Explicit even though it is currently equal.
    document.patch_style<base_theme>({.font_size = 24});
    REQUIRE(base.style().font_size == 20);
    base.clear_style_overrides();
    REQUIRE(base.style().font_size == 24);
    document.patch_style<base_theme>({.width = centui::px(80)});
    REQUIRE(base.style().font_size == 24);
    REQUIRE(base.style().width == centui::px(80));
    document.clear_style_patch<base_theme>();
    REQUIRE(base.style().font_size == 16);
    REQUIRE(base.style().width == centui::px(40));
    document.set_style({});
    REQUIRE(root.style().font_size == centui::style{}.font_size);
}

TEST_CASE("Full style replacements can reveal their original recipe again", "[style][widget]")
{
    centui::style replacement;
    replacement.font_size = 30;
    auto value = centui::container<base_theme>{replacement};
    centui::document document{value};
    document.patch_style<base_theme>({.font_size = 22});
    REQUIRE(value.style().font_size == 30);
    value.clear_style_overrides();
    REQUIRE(value.style().font_size == 22);
    value.set_style(replacement);
    document.set_style({.font_size = 10});
    REQUIRE(value.style().font_size == 30);
    value.clear_style_overrides();
    REQUIRE(value.style().font_size == 22);
}

TEST_CASE("Document patches reach future widgets and follow mount ownership", "[style][document]")
{
    auto root = centui::container{};
    centui::document first{root};
    first.patch_style<base_theme>({.font_size = 25});
    auto child = centui::container<base_theme>{};
    root->append(child);
    REQUIRE(child.style().font_size == 25);
    first.remove(child);
    REQUIRE(child.style().font_size == 16);

    {
        centui::document second;
        second.patch_style<base_theme>({.font_size = 32});
        second.set_root(child);
        REQUIRE(child.style().font_size == 32);
    }
    REQUIRE(child.style().font_size == 16);
}

TEST_CASE("Invalid document updates and mounts leave prior state intact", "[style][document]")
{
    auto first = centui::container<base_theme>{};
    auto second = centui::container<base_theme>{};
    centui::document document{centui::container{first, second}};
    REQUIRE_THROWS_AS(document.patch_style<base_theme>({.font_size = -1}), std::invalid_argument);
    REQUIRE(first.style().font_size == 16);
    REQUIRE(second.style().font_size == 16);
    auto later = centui::container<base_theme>{};
    document.append(document.root(), later);
    REQUIRE(later.style().font_size == 16);

    centui::document destination;
    destination.patch_style<base_theme>({.font_size = -1});
    auto detached = centui::container<base_theme>{};
    REQUIRE_THROWS_AS(destination.set_root(detached), std::invalid_argument);
    REQUIRE(destination.empty());
    REQUIRE(detached.style().font_size == 16);
    REQUIRE_FALSE(detached->parent().valid());
}

TEST_CASE("Document style changes invalidate only actual resolved differences", "[style][document]")
{
    auto value = centui::container<base_theme>{};
    centui::document document{value};
    (void)document.prepare({100, 100});
    document.patch_style<base_theme>({.font_size = 16});
    REQUIRE_FALSE(document.pending_changes().any());
    document.patch_style<base_theme>({.text_color = centui::states_color::all({20, 30, 40, 255})});
    REQUIRE(document.pending_changes().paint);
    REQUIRE_FALSE(document.pending_changes().layout);
    (void)document.prepare({100, 100});
    document.patch_style<base_theme>({.width = centui::px(60)});
    REQUIRE(document.pending_changes().layout);
}

TEST_CASE("Context callbacks expose the emitting widget and support self replacement", "[event][widget]")
{
    auto value = centui::make_widget<activation_probe>();
    auto root = centui::container{value};
    centui::document document{root};
    int calls = 0;
    value->set_on_click(
        [&](centui::event& event)
        {
            REQUIRE(&event.document() == &document);
            REQUIRE(event.target() == centui::handle<>{value});
            REQUIRE(event.target()->parent() == centui::handle<>{root});
            REQUIRE_FALSE(event.input());
            ++calls;
            value->set_on_click(
                [&]
                {
                    calls += 10;
                }
            );
        }
    );
    value->activate();
    REQUIRE(calls == 1);
    value->activate();
    REQUIRE(calls == 11);
}

TEST_CASE("Context callbacks borrow originating input only during dispatch", "[event][document]")
{
    auto value = centui::make_widget<activation_probe>();
    centui::document document{value};
    bool received_input = false;
    value->set_on_click(
        [&](centui::event& event)
        {
            REQUIRE(event.input());
            REQUIRE(event.input()->get().type == centui::input_event_type::key_pressed);
            received_input = true;
        }
    );
    REQUIRE(value->request_focus());
    document.process_event({.type = centui::input_event_type::key_pressed, .code = centui::key::enter}, {100, 100});
    REQUIRE(received_input);
    value->set_on_click(
        [](centui::event& event)
        {
            REQUIRE_FALSE(event.input());
        }
    );
    value->activate();
}

TEST_CASE("Optional callback context precedes widget-specific arguments", "[event][widget]")
{
    auto value = centui::make_widget<activation_probe>();
    centui::document document{value};
    float received = 0;
    centui::callback<float> callback = [&](centui::event& event, float number)
    {
        REQUIRE(event.target() == centui::handle<>{value});
        received = number;
    };
    callback(*value, 0.75f);
    REQUIRE(received == 0.75f);
    callback = [&](float number)
    {
        received = number;
    };
    callback(*value, 0.25f);
    REQUIRE(received == 0.25f);
    callback = [](auto&&...)
    {
        FAIL("An empty callback should not invoke");
    };
    callback = nullptr;
    callback(*value, 1);
}
