#include "text_analysis.hpp"
#include "unicode_analysis.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <centypo/unicode.hpp>

namespace
{

void append(std::string& value, std::uint32_t scalar)
{
    if(scalar < 0x80)
    {
        value += static_cast<char>(scalar);
    }
    else if(scalar < 0x800)
    {
        value += static_cast<char>(0xc0 | (scalar >> 6));
        value += static_cast<char>(0x80 | (scalar & 63));
    }
    else if(scalar < 0x10000)
    {
        value += static_cast<char>(0xe0 | (scalar >> 12));
        value += static_cast<char>(0x80 | ((scalar >> 6) & 63));
        value += static_cast<char>(0x80 | (scalar & 63));
    }
    else
    {
        value += static_cast<char>(0xf0 | (scalar >> 18));
        value += static_cast<char>(0x80 | ((scalar >> 12) & 63));
        value += static_cast<char>(0x80 | ((scalar >> 6) & 63));
        value += static_cast<char>(0x80 | (scalar & 63));
    }
}

} // namespace

TEST_CASE("Unicode 17 bidi character conformance uses UTF-8 native offsets", "[text][bidi][conformance]")
{
    std::ifstream source(CENTUI_BIDI_TEST_PATH);
    REQUIRE(source.is_open());
    std::string line;
    std::size_t number = 0, checked = 0;
    while(std::getline(source, line))
    {
        ++number;
        if(line.empty() || line[0] == '#')
        {
            continue;
        }
        std::vector<std::string> fields;
        std::istringstream columns(line);
        std::string field;
        while(std::getline(columns, field, ';'))
        {
            fields.push_back(field);
        }
        if(fields.size() < 5)
        {
            continue;
        }
        std::string text, token;
        std::istringstream points(fields[0]);
        bool nul = false;
        while(points >> token)
        {
            const auto scalar = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
            nul |= scalar == 0;
            append(text, scalar);
        }
        if(nul || text.empty())
        {
            continue;
        }
        const auto scalars = centypo::details::decode_scalars(text);
        const auto direction = std::stoi(fields[1]);
        centypo::details::text_analysis analysis{
            text,
            {},
            direction == 2   ? centypo::text_direction::automatic
            : direction == 1 ? centypo::text_direction::rtl
                             : centypo::text_direction::ltr
        };
        INFO("BidiCharacterTest line " << number);
        REQUIRE(analysis.rtl(0) == (std::stoi(fields[2]) != 0));
        std::vector<bool> removed;
        std::vector<int> expected_levels;
        std::istringstream levels(fields[3]);
        while(levels >> token)
        {
            removed.push_back(token == "x");
            expected_levels.push_back(token == "x" ? -1 : std::stoi(token));
        }
        const auto runs = analysis.line_runs(0, text.size());
        std::vector<std::size_t> order;
        for(std::size_t run = 0; run < runs.size(); ++run)
        {
            std::vector<std::size_t> indices;
            for(std::size_t i = 0; i < scalars.size(); ++i)
            {
                if(!removed[i] && scalars[i].begin >= runs[run].offset &&
                   scalars[i].begin < runs[run].offset + runs[run].length)
                {
                    REQUIRE(runs[run].level == expected_levels[i]);
                    indices.push_back(i);
                }
            }
            if(runs[run].level & 1)
            {
                std::reverse(indices.begin(), indices.end());
            }
            order.insert(order.end(), indices.begin(), indices.end());
        }
        std::vector<std::size_t> expected;
        std::istringstream positions(fields[4]);
        std::size_t at;
        while(positions >> at)
        {
            expected.push_back(at);
        }
        REQUIRE(order == expected);
        ++checked;
    }
    REQUIRE(checked > 1000);
}
