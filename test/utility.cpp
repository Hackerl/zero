#include "catch_extensions.h"
#include <zero/utility.h>

TEST_CASE("flatten", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(zero::flatten(std::expected<std::expected<void, short>, int>{}));
        }

        SECTION("has error") {
            REQUIRE_ERROR(zero::flatten(std::expected<std::expected<void, short>, int>{std::unexpected{-1}}), -1);
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(zero::flatten(std::expected<std::expected<int, short>, int>{0}) == 0);
        }

        SECTION("has error") {
            REQUIRE_ERROR(zero::flatten(std::expected<std::expected<int, short>, int>{std::unexpected{-1}}), -1);
        }
    }
}

TEST_CASE("flatten with error type", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(zero::flattenWith<long>(std::expected<std::expected<void, short>, int>{}));
        }

        SECTION("has error") {
            REQUIRE_ERROR(
                zero::flattenWith<long>(std::expected<std::expected<void, short>, int>{std::unexpected{-1}}),
                -1
            );
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(zero::flattenWith<long>(std::expected<std::expected<int, short>, int>{0}) == 0);
        }

        SECTION("has error") {
            REQUIRE_ERROR(
                zero::flattenWith<long>(std::expected<std::expected<int, short>, int>{
                    std::unexpected{-1}
                }),
                -1
            );
        }
    }
}
