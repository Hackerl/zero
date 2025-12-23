#include "catch_extensions.h"
#include <zero/env.h>
#include <zero/defer.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>

TEST_CASE("get environment variable", "[env]") {
    const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("environment variable does not exist") {
        REQUIRE(zero::env::get(name) == std::nullopt);
    }

    SECTION("environment variable exist") {
        const auto value = GENERATE(take(1, randomAlphanumericString(8, 64)));
        zero::env::set(name, value);
        Z_DEFER(zero::env::unset(name));
        REQUIRE(zero::env::get(name) == value);
    }
}

TEST_CASE("set environment variable", "[env]") {
    const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto value = GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("environment variable does not exist") {
        zero::env::set(name, value);
        Z_DEFER(zero::env::unset(name));
        REQUIRE(zero::env::get(name) == value);
    }

    SECTION("environment variable exist") {
        zero::env::set(name, "1");
        Z_DEFER(zero::env::unset(name));
        zero::env::set(name, value);
        REQUIRE(zero::env::get(name) == value);
    }
}

TEST_CASE("unset environment variable", "[env]") {
    const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("environment variable does not exist") {
        REQUIRE_NOTHROW(zero::env::unset(name));
    }

    SECTION("environment variable exist") {
        const auto value = GENERATE(take(1, randomAlphanumericString(8, 64)));
        zero::env::set(name, value);
        REQUIRE(zero::env::get(name) == value);
        zero::env::unset(name);
        REQUIRE(zero::env::get(name) == std::nullopt);
    }
}

TEST_CASE("list environment variable", "[env]") {
    const auto name = GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto value = GENERATE(take(1, randomAlphanumericString(8, 64)));

    zero::env::set(name, value);
    Z_DEFER(zero::env::unset(name));

    const auto envs = zero::env::list();
    REQUIRE_THAT(envs | std::views::keys, Catch::Matchers::Contains(name));
    REQUIRE(envs.at(name) == value);
}
