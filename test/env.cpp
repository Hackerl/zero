#include <zero/env.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>

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

TEST_CASE("list environment variable", "[env]") {
    REQUIRE(zero::env::set("ZERO_ENV_LIST", "1"));
    DEFER(REQUIRE(zero::env::unset("ZERO_ENV_LIST")));

    const auto envs = zero::env::list();
    REQUIRE(envs);
    REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_ENV_LIST"));
    REQUIRE(envs->at("ZERO_ENV_LIST") == "1");
}
