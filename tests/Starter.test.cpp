#include <catch2/catch_test_macros.hpp>

import std;

TEST_CASE("starter", "[tag]")
{
    std::string hello = "Hello from starter!";

    REQUIRE(hello == hello);
}
