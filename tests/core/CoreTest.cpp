#include <catch2/catch_test_macros.hpp>

TEST_CASE("trivial sanity check", "[core]")
{
    REQUIRE(1+1 == 2);
}