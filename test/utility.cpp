#include <zero/utility.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("flatten", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            const auto result = zero::flatten(std::expected<std::expected<void, short>, int>{});
            REQUIRE(result);
        }

        SECTION("has error") {
            const auto result = zero::flatten(std::expected<std::expected<void, short>, int>{std::unexpected{-1}});
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == -1);
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            const auto result = zero::flatten(std::expected<std::expected<int, short>, int>{0});
            REQUIRE(result);
            REQUIRE(*result == 0);
        }

        SECTION("has error") {
            const auto result = zero::flatten(std::expected<std::expected<int, short>, int>{std::unexpected{-1}});
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == -1);
        }
    }
}

TEST_CASE("flatten with error type", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            const auto result = zero::flattenWith<long>(std::expected<std::expected<void, short>, int>{});
            REQUIRE(result);
        }

        SECTION("has error") {
            const auto result = zero::flattenWith<long>(std::expected<std::expected<void, short>, int>{
                std::unexpected{-1}
            });
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == -1);
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            const auto result = zero::flattenWith<long>(std::expected<std::expected<int, short>, int>{0});
            REQUIRE(result);
            REQUIRE(*result == 0);
        }

        SECTION("has error") {
            const auto result = zero::flattenWith<long>(std::expected<std::expected<int, short>, int>{
                std::unexpected{-1}
            });
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == -1);
        }
    }
}
