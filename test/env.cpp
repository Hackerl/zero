#include <zero/env.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("environment variables", "[env]") {
    auto value = zero::env::get("ZERO_ENV_VARIABLE");
    REQUIRE(value);
    REQUIRE_FALSE(*value);

    REQUIRE(zero::env::set("ZERO_ENV_VARIABLE", "1"));

    value = zero::env::get("ZERO_ENV_VARIABLE");
    REQUIRE(value);
    REQUIRE(*value);
    REQUIRE(**value == "1");

    REQUIRE(zero::env::unset("ZERO_ENV_VARIABLE"));

    value = zero::env::get("ZERO_ENV_VARIABLE");
    REQUIRE(value);
    REQUIRE_FALSE(*value);
}
