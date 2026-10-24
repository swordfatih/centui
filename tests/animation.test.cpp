#include <functional>
#include <limits>

#include <centui/centui.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

class animated final : public centui::widget
{
public:
    using widget::request_frame;
    int ticks = 0;
    centui::frame_duration elapsed{};
    std::function<void()> callback;

protected:
    void on_frame(centui::frame_duration duration) override
    {
        ++ticks;
        elapsed = duration;
        if(callback)
        {
            callback();
        }
    }
};

} // namespace

TEST_CASE("Frame requests coalesce and callbacks schedule only the following tick", "[animation]")
{
    auto leaf = centui::make_widget<animated>();
    leaf->request_frame(); // Detached requests survive mounting.
    leaf->request_frame();
    centui::document document{leaf};
    leaf->callback = [weak = leaf.weak()]
    {
        weak.lock()->request_frame();
    };
    document.advance(centui::frame_duration{0.25f});
    REQUIRE(leaf->ticks == 1);
    REQUIRE(leaf->elapsed.count() == 0.25f);
    document.advance(centui::frame_duration{0.5f});
    REQUIRE(leaf->ticks == 2);
    leaf->callback = {};
    document.advance(centui::frame_duration{0});
    document.advance(centui::frame_duration{0});
    REQUIRE(leaf->ticks == 3);
}

TEST_CASE("Scheduled frames do not retain detached widgets", "[animation]")
{
    auto root = centui::container{};
    centui::document document{root};
    auto leaf = centui::make_widget<animated>();
    auto weak = leaf.weak();
    root->append(leaf);
    leaf->request_frame();
    root->remove(leaf);
    leaf = {};
    REQUIRE_FALSE(weak.lock().valid());
    REQUIRE_NOTHROW(document.advance(centui::frame_duration{1}));
}

TEST_CASE("Frame failures preserve other requests and release dispatch guards", "[animation]")
{
    auto first = centui::make_widget<animated>();
    auto second = centui::make_widget<animated>();
    auto root = centui::container{first, second};
    centui::document document{root};
    first->request_frame();
    second->request_frame();
    first->callback = []
    {
        throw std::runtime_error("frame failed");
    };
    REQUIRE_THROWS_AS(document.advance(centui::frame_duration{1}), std::runtime_error);
    REQUIRE(second->ticks == 0);
    REQUIRE_NOTHROW(document.advance(centui::frame_duration{1}));
    REQUIRE(first->ticks == 1);
    REQUIRE(second->ticks == 1);
    REQUIRE_THROWS_AS(document.advance(centui::frame_duration{-1}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        document.advance(centui::frame_duration{std::numeric_limits<float>::infinity()}),
        std::invalid_argument
    );
}

TEST_CASE("Frame callbacks commit tree edits after the tick batch", "[animation]")
{
    auto first = centui::make_widget<animated>();
    auto second = centui::make_widget<animated>();
    auto root = centui::container{first, second};
    centui::document document{root};
    first->callback = [root, second]() mutable
    {
        root->remove(second);
    };
    first->request_frame();
    second->request_frame();
    document.advance(centui::frame_duration{1});
    REQUIRE(second->ticks == 1);
    REQUIRE_FALSE(second->parent().valid());
    REQUIRE(root->children().size() == 1);
    second->request_frame();
    document.advance(centui::frame_duration{1});
    REQUIRE(second->ticks == 1);
    root->append(second);
    document.advance(centui::frame_duration{1});
    REQUIRE(second->ticks == 2);
    first->callback = {};
}
