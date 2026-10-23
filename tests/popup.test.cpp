
#include <centui/widgets/popup.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone popup header retains content and close callback", "[widget][popup][header]")
{
    auto content = centui::container<>();
    auto control = centui::popup{content};
    int closes = 0;
    control->set_on_close(
        [&]
        {
            ++closes;
        }
    );
    REQUIRE(content->parent() == centui::handle<>{control});
    REQUIRE(control->style().visibility == centui::visibility::collapsed);
    control->open();
    REQUIRE(control->style().visibility == centui::visibility::visible);
    control->close();
    control->close();
    REQUIRE(control->style().visibility == centui::visibility::collapsed);
    REQUIRE(closes == 1);
}
