#include "catch_extensions.h"
#include <zero/utility.h>

TEST_CASE("flatten std::optional", "[utility]") {
    SECTION("has value") {
        REQUIRE(zero::flatten(std::optional<std::optional<int>>{0}) == 0);
    }

    SECTION("no value") {
        REQUIRE_FALSE(zero::flatten(std::optional<std::optional<int>>{}));
        REQUIRE_FALSE(zero::flatten(std::optional<std::optional<int>>{std::optional<int>{}}));
    }
}

TEST_CASE("flatten std::expected", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(zero::flatten(std::expected<std::expected<void, short>, int>{}));
        }

        SECTION("has error") {
            REQUIRE_ERROR(zero::flatten(std::expected<std::expected<void, short>, int>{std::unexpect, -1}), -1);
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(zero::flatten(std::expected<std::expected<int, short>, int>{0}) == 0);
        }

        SECTION("has error") {
            REQUIRE_ERROR(zero::flatten(std::expected<std::expected<int, short>, int>{std::unexpect, -1}), -1);
        }
    }
}

TEST_CASE("flatten std::expected with error type", "[utility]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(zero::flattenWith<long>(std::expected<std::expected<void, short>, int>{}));
        }

        SECTION("has error") {
            REQUIRE_ERROR(
                zero::flattenWith<long>(std::expected<std::expected<void, short>, int>{std::unexpect, -1}),
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
                zero::flattenWith<long>(std::expected<std::expected<int, short>, int>{std::unexpect, -1}),
                -1
            );
        }
    }
}

TEST_CASE("extract std::expected", "[utility]") {
    SECTION("has value") {
        REQUIRE(zero::extract(std::expected<int, int>{0}) == 0);
    }

    SECTION("has error") {
        REQUIRE_FALSE(zero::extract(std::expected<int, int>{std::unexpect, -1}));
    }
}

TEST_CASE("transpose std::expected", "[utility]") {
    SECTION("has value") {
        SECTION("has value") {
            REQUIRE(zero::transpose(std::expected<std::optional<int>, int>{0}) == 0);
        }

        SECTION("no value") {
            REQUIRE_FALSE(zero::transpose(std::expected<std::optional<int>, int>{}));
        }
    }

    SECTION("has error") {
        REQUIRE(zero::transpose(std::expected<std::optional<int>, int>{std::unexpect, -1}) == std::unexpected{-1} );
    }
}

TEST_CASE("transpose std::optional", "[utility]") {
    SECTION("has value") {
        SECTION("has value") {
            REQUIRE(zero::transpose(std::optional<std::expected<int, int>>{0}) == 0);
        }

        SECTION("has error") {
            REQUIRE(
                zero::transpose(std::optional<std::expected<int, int>>{std::unexpected{-1}}) == std::unexpected{-1}
            );
        }
    }

    SECTION("no value") {
        REQUIRE(zero::transpose(std::optional<std::expected<int, int>>{}) == std::nullopt);
    }
}
