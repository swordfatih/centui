#include <catch2/catch_test_macros.hpp>

import starter;
import std;

TEST_CASE("starter", "[tag]")
{
    std::string hello = "Hello from starter!";

    REQUIRE(starter::starter::to_string() == hello);
}
