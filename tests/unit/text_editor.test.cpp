#include <catch2/catch_test_macros.hpp>
#include <centypo/editor.hpp>

TEST_CASE("Composition is provisional and commit restores directional selection on undo", "[input][editor]")
{
    centypo::editor editor{true};
    editor.set_text("original");
    const centypo::text_position anchor{8, centypo::text_affinity::upstream}, caret{0};
    editor.select(anchor, caret);
    editor.compose("draft", 3, 2);
    REQUIRE(editor.value() == "original");
    REQUIRE(editor.display_text() == "draft");
    REQUIRE(editor.display_caret() == 3);
    REQUIRE(editor.composition_selection() == std::pair<std::size_t, std::size_t>{3, 5});
    REQUIRE_THROWS_AS(editor.compose(std::string("x\0y", 3), 1), std::invalid_argument);
    REQUIRE(editor.display_text() == "draft");
    REQUIRE(editor.replace("committed", true));
    REQUIRE(editor.value() == "committed");
    REQUIRE(editor.undo(false));
    REQUIRE(editor.value() == "original");
    REQUIRE(editor.anchor() == anchor);
    REQUIRE(editor.caret() == caret);
    REQUIRE(editor.undo(true));
    REQUIRE(editor.value() == "committed");
}

TEST_CASE("Typing groups end on selection and composition cancellation does not enter history", "[input][editor]")
{
    centypo::editor editor{false};
    editor.replace("x", true);
    editor.replace("y", true);
    REQUIRE(editor.undo(false));
    REQUIRE(editor.value().empty());
    REQUIRE(editor.undo(true));
    REQUIRE(editor.value() == "xy");
    editor.select({0}, {0});
    editor.replace("z", true);
    REQUIRE(editor.undo(false));
    REQUIRE(editor.value() == "xy");
    editor.set_text("A");
    editor.compose("preview", 7);
    editor.cancel_composition();
    REQUIRE(editor.value() == "A");
    REQUIRE_FALSE(editor.undo(false));
    REQUIRE_THROWS_AS(editor.replace("\xc0\x80"), std::invalid_argument);
    REQUIRE(editor.value() == "A");
}

TEST_CASE("Word commands preserve graphemes when Unicode word boundaries differ", "[input][editor][unicode]")
{
    const std::string text = "a\xf0\x9f\x87\xa6\xe2\x80\x8d\xf0\x9f\x87\xa7\xf0\x9f\x87\xa8"
                             "b";
    const auto boundaries = centypo::analyze_text(text);
    REQUIRE(boundaries.words == std::vector<std::size_t>{0, 1, 12, 16, 17});
    REQUIRE(boundaries.graphemes == std::vector<std::size_t>{0, 1, 8, 16, 17});

    centypo::editor editor;
    editor.set_text(text);

    SECTION("forward")
    {
        editor.select({1}, {1});
        REQUIRE(editor.adjacent(true, true) == 16);
        REQUIRE(editor.erase(true, true));
    }

    SECTION("backward")
    {
        editor.select({16}, {16});
        REQUIRE(editor.adjacent(false, true) == 1);
        REQUIRE(editor.erase(false, true));
    }

    REQUIRE(editor.value() == "ab");
    REQUIRE(editor.caret().offset == 1);
    REQUIRE(editor.undo(false));
    REQUIRE(editor.value() == text);
}

TEST_CASE("Editing preserves hard-break encoding and deletes CRLF as one grapheme", "[input][editor]")
{
    centypo::editor multiline{true};
    multiline.set_text("A\r\nB\tC");
    multiline.select({3}, {3});
    REQUIRE(multiline.adjacent(false, false) == 1);
    REQUIRE(multiline.erase(false, false));
    REQUIRE(multiline.value() == "AB\tC");
    REQUIRE(multiline.undo(false));
    REQUIRE(multiline.value() == "A\r\nB\tC");
    centypo::editor single{false};
    single.replace(
        "A\r\nB\tC\xe2\x80\xa8"
        "D"
    );
    REQUIRE(single.value() == "A B\tC D");
    single.select({0}, {single.value().size()});
    single.compose("x\r\ny", 3);
    REQUIRE(single.display_text() == "x y");
    REQUIRE(single.display_caret() == 2);
}

TEST_CASE("Edits that merge graphemes keep the committed caret at a boundary", "[input][editor][unicode]")
{
    centypo::editor editor;
    std::string expected;

    SECTION("insert before a leading combining mark")
    {
        editor.set_text("\xcc\x81");
        editor.select({0}, {0});
        REQUIRE(editor.replace("a"));
        expected = "a\xcc\x81";
    }
    SECTION("insert a joiner between emoji")
    {
        editor.set_text("\xf0\x9f\x91\xa9\xf0\x9f\x91\xa9");
        editor.select({4}, {4});
        REQUIRE(editor.replace("\xe2\x80\x8d"));
        expected = "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9";
    }
    SECTION("delete between regional indicators")
    {
        editor.set_text("\xf0\x9f\x87\xa6 \xf0\x9f\x87\xa7");
        editor.select({4}, {5});
        REQUIRE(editor.erase(false, false));
        expected = "\xf0\x9f\x87\xa6\xf0\x9f\x87\xa7";
    }

    REQUIRE(editor.value() == expected);
    REQUIRE(editor.anchor().offset == expected.size());
    REQUIRE(editor.caret().offset == expected.size());
    REQUIRE(editor.undo(false));
    REQUIRE(editor.undo(true));
    REQUIRE(editor.value() == expected);
    REQUIRE(editor.caret().offset == expected.size());
    REQUIRE(editor.erase(false, false));
    REQUIRE(editor.value().empty());
}
