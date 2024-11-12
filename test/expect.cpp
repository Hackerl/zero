#include <zero/expect.h>
#include <catch2/catch_test_macros.hpp>

tl::expected<int, std::error_code> twice(const int value) {
    if (value % 2)
        return tl::unexpected{make_error_code(std::errc::invalid_argument)};

    return value * 2;
}

TEST_CASE("expect macro", "[expect]") {
    const auto calculate = [](const int input) -> tl::expected<int, std::error_code> {
        auto value = twice(input);
        EXPECT(value);

        value = twice(*value);
        EXPECT(value);

        return *value;
    };

    SECTION("valid") {
        const auto result = calculate(2);
        REQUIRE(result);
        REQUIRE(*result == 8);
    }

    SECTION("invalid") {
        const auto result = calculate(1);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::invalid_argument);
    }
}

#ifdef __GNUC__
TEST_CASE("try macro", "[expect]") {
    const auto calculate = [](const int input) -> tl::expected<int, std::error_code> {
        auto value = TRY(twice(input));
        value = TRY(twice(value));
        return value;
    };

    SECTION("valid") {
        const auto result = calculate(2);
        REQUIRE(result);
        REQUIRE(*result == 8);
    }

    SECTION("invalid") {
        const auto result = calculate(1);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::invalid_argument);
    }
}
#endif
