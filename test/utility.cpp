#include "catch_extensions.h"
#include <zero/utility.h>

TEST_CASE("flatten", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(zero::flatten(tl::expected<tl::expected<void, short>, int>{}));
        }

        SECTION("has error") {
            REQUIRE_ERROR(zero::flatten(tl::expected<tl::expected<void, short>, int>{tl::unexpected{-1}}), -1);
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(zero::flatten(tl::expected<tl::expected<int, short>, int>{0}) == 0);
        }

        SECTION("has error") {
            REQUIRE_ERROR(zero::flatten(tl::expected<tl::expected<int, short>, int>{tl::unexpected{-1}}), -1);
        }
    }
}

TEST_CASE("flatten with error type", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(zero::flattenWith<long>(tl::expected<tl::expected<void, short>, int>{}));
        }

        SECTION("has error") {
            REQUIRE_ERROR(
                zero::flattenWith<long>(tl::expected<tl::expected<void, short>, int>{tl::unexpected{-1}}),
                -1
            );
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(zero::flattenWith<long>(tl::expected<tl::expected<int, short>, int>{0}) == 0);
        }

        SECTION("has error") {
            REQUIRE_ERROR(
                zero::flattenWith<long>(tl::expected<tl::expected<int, short>, int>{
                    tl::unexpected{-1}
                }),
                -1
            );
        }
    }
}
