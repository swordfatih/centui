#include <functional>
#include <limits>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class drawing final : public centui::widget
{
public:
    drawing()
    {
        centui::style appearance;
        appearance.width = centui::px(40);
        appearance.height = centui::px(30);
        set_style(appearance);
        shape.rectangle({2, 2, 20, 20});
    }

    void refresh()
    {
        invalidate_paint();
    }

    centui::path shape;
    centui::brush brush = centui::color{255, 0, 0, 255};
    float stroke_width = 0;
    std::function<void()> layout_callback;

protected:
    void on_layout() override
    {
        if(layout_callback)
        {
            layout_callback();
        }
    }

    void paint(centui::painter& output) const override
    {
        if(stroke_width != 0)
        {
            output.stroke(shape, brush, stroke_width);
        }
        else
        {
            output.fill(shape, brush);
        }
    }
};

} // namespace

TEST_CASE("Vector rasters reuse masks and survive eviction and document destruction", "[paint]")
{
    auto leaf = centui::make_widget<drawing>();
    std::shared_ptr<const centui::bitmap> retained;
    {
        centui::document document{leaf};
        const auto first = document.prepare({100, 100});
        REQUIRE(first.commands.size() == 1);
        retained = first.commands.front().image;
        REQUIRE(retained);
        centui::validate_bitmap(*retained);
        leaf->brush = centui::color{0, 255, 0, 255};
        leaf->refresh();
        const auto second = document.prepare({100, 100});
        REQUIRE(second.commands.front().image == retained);
        REQUIRE(second.commands.front().color.g == 255);

        document.set_paint_cache_budget(0);
        leaf->refresh();
        REQUIRE(document.prepare({100, 100}).commands.front().image != retained);
    }
    REQUIRE_FALSE(retained->pixels.empty());
}

TEST_CASE("Copied paths keep their geometry when an alias is edited", "[paint]")
{
    auto leaf = centui::make_widget<drawing>();
    centui::document document{leaf};
    const auto original = document.prepare({100, 100});
    auto alias = leaf->shape;
    alias.rectangle({30, 2, 5, 5});
    leaf->refresh();
    REQUIRE(document.prepare({100, 100}).commands.front().image == original.commands.front().image);
    leaf->shape = alias;
    leaf->refresh();
    const auto edited = document.prepare({100, 100});
    REQUIRE(edited.commands.front().image != original.commands.front().image);
    REQUIRE(edited.commands.front().bounds.width > original.commands.front().bounds.width);
}

TEST_CASE("Editing a cached path refreshes its raster without changing bounds", "[paint]")
{
    auto leaf = centui::make_widget<drawing>();
    leaf->shape = {};
    leaf->shape.move_to({2, 2}).line_to({22, 2}).line_to({2, 22}).close();
    centui::document document{leaf};
    const auto original = document.prepare({100, 100});
    REQUIRE(original.commands.size() == 1);

    leaf->shape.rectangle({2, 2, 20, 20});
    leaf->refresh();
    const auto edited = document.prepare({100, 100});
    REQUIRE(edited.commands.size() == 1);
    REQUIRE(edited.commands.front().bounds == original.commands.front().bounds);
    REQUIRE(edited.commands.front().image != original.commands.front().image);
    REQUIRE(edited.commands.front().image->pixels != original.commands.front().image->pixels);
    leaf->refresh();
    REQUIRE(document.prepare({100, 100}).commands.front().image == edited.commands.front().image);
}

TEST_CASE("Vector gradients rasterize at viewport scale and validate stops", "[paint]")
{
    auto leaf = centui::make_widget<drawing>();
    leaf->brush = centui::linear_gradient{{2, 2}, {22, 2}, {{0, {255, 0, 0, 255}}, {1, {0, 0, 255, 255}}}};
    centui::document document{leaf};
    const auto first = document.prepare({100, 100});
    const auto image = first.commands.front().image;
    const auto pixel = [&](int x, int channel)
    {
        return image->pixels[(static_cast<std::size_t>(image->height / 2) * image->width + x) * 4 + channel];
    };
    REQUIRE(pixel(4, 0) > pixel(4, 2));
    REQUIRE(pixel(image->width - 5, 2) > pixel(image->width - 5, 0));
    const auto scaled = document.prepare({200, 200, 2});
    REQUIRE(scaled.commands.front().image->width == image->width * 2 - 2);

    leaf->brush = centui::linear_gradient{{0, 0}, {20, 0}, {{0.8f, {}}, {0.2f, {}}}};
    leaf->refresh();
    REQUIRE_THROWS_AS(document.prepare({100, 100}), std::invalid_argument);
    leaf->brush = centui::radial_gradient{{10, 10}, 0, {{0, {}}, {1, {}}}};
    REQUIRE_THROWS_AS(document.prepare({100, 100}), std::invalid_argument);
    leaf->brush = centui::radial_gradient{{10, 10}, 10, {{0, {255, 255, 255, 255}}, {1, {0, 0, 0, 255}}}};
    REQUIRE_FALSE(document.prepare({100, 100}).commands.empty());
}

TEST_CASE("Invalid vector geometry and stroke widths fail before drawing", "[paint]")
{
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    centui::path path;
    REQUIRE_THROWS_AS(path.move_to({nan, 0}), std::invalid_argument);
    REQUIRE_THROWS_AS(path.rectangle({0, 0, -1, 10}), std::invalid_argument);
    REQUIRE_THROWS_AS(path.circle({0, 0}, -1), std::invalid_argument);
    REQUIRE_THROWS_AS(path.rounded_rectangle({0, 0, 10, 10}, nan), std::invalid_argument);
    auto leaf = centui::make_widget<drawing>();
    leaf->stroke_width = -1;
    centui::document document{leaf};
    REQUIRE_THROWS_AS(document.prepare({100, 100}), std::invalid_argument);
    leaf->stroke_width = 2;
    REQUIRE_FALSE(document.prepare({100, 100}).commands.empty());
}

TEST_CASE("Failed layout hooks retry retained geometry preparation", "[paint][layout]")
{
    auto leaf = centui::make_widget<drawing>();
    int calls = 0;
    leaf->layout_callback = [&]
    {
        if(++calls == 1)
        {
            throw std::runtime_error("geometry unavailable");
        }
    };
    centui::document document{leaf};
    REQUIRE_THROWS_AS(document.prepare({100, 100}), std::runtime_error);
    REQUIRE_FALSE(document.prepare({100, 100}).commands.empty());
    REQUIRE(calls == 2);
}
