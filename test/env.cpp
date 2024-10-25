#include <zero/env.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("get environment variable", "[env]") {
    SECTION("environment variable does not exist") {
        const auto value = zero::env::get("ZERO_ENV_GET");
        REQUIRE(value);
        REQUIRE_FALSE(*value);
    }

    SECTION("environment variable exist") {
        REQUIRE(zero::env::set("ZERO_ENV_GET", "1"));
        DEFER(REQUIRE(zero::env::unset("ZERO_ENV_GET")));

        const auto value = zero::env::get("ZERO_ENV_GET");
        REQUIRE(value);
        REQUIRE(*value);
        REQUIRE(**value == "1");
    }
}

TEST_CASE("set environment variable", "[env]") {
    SECTION("environment variable does not exist") {
        REQUIRE(zero::env::set("ZERO_ENV_SET", "1"));
        DEFER(REQUIRE(zero::env::unset("ZERO_ENV_SET")));

        const auto value = zero::env::get("ZERO_ENV_SET");
        REQUIRE(value);
        REQUIRE(*value);
        REQUIRE(**value == "1");
    }

    SECTION("environment variable exist") {
        REQUIRE(zero::env::set("ZERO_ENV_SET", "1"));
        DEFER(REQUIRE(zero::env::unset("ZERO_ENV_SET")));
        REQUIRE(zero::env::set("ZERO_ENV_SET", "2"));

        const auto value = zero::env::get("ZERO_ENV_SET");
        REQUIRE(value);
        REQUIRE(*value);
        REQUIRE(**value == "2");
    }
}

TEST_CASE("unset environment variable", "[env]") {
    SECTION("environment variable does not exist") {
        REQUIRE(zero::env::unset("ZERO_ENV_UNSET"));
    }

    SECTION("environment variable exist") {
        REQUIRE(zero::env::set("ZERO_ENV_UNSET", "1"));

        auto value = zero::env::get("ZERO_ENV_UNSET");
        REQUIRE(value);
        REQUIRE(*value);
        REQUIRE(**value == "1");

        REQUIRE(zero::env::unset("ZERO_ENV_UNSET"));

        value = zero::env::get("ZERO_ENV_UNSET");
        REQUIRE(value);
        REQUIRE_FALSE(*value);
    }
}
