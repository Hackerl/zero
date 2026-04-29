#include "catch_extensions.h"
#include <zero/expect.h>

namespace {
    std::expected<int, std::error_code> twice(const int value) {
        if (value % 2)
            return std::unexpected{make_error_code(std::errc::invalid_argument)};

        return value * 2;
    }
}

TEST_CASE("propagate std::expected error with early return", "[expect]") {
    const auto calculate = [](const int input) -> std::expected<int, std::error_code> {
        auto value = twice(input);
        Z_EXPECT(value);

        value = twice(*value);
        Z_EXPECT(value);

        return *value;
    };

    SECTION("valid") {
        REQUIRE(calculate(2) == 8);
    }

    SECTION("invalid") {
        REQUIRE_ERROR(calculate(1), std::errc::invalid_argument);
    }
}

#ifdef __GNUC__
TEST_CASE("unwrap value or propagate error", "[expect]") {
    const auto calculate = [](const int input) -> std::expected<int, std::error_code> {
        auto value = Z_TRY(twice(input));
        value = Z_TRY(twice(value));
        return value;
    };

    SECTION("valid") {
        REQUIRE(calculate(2) == 8);
    }

    SECTION("invalid") {
        REQUIRE_ERROR(calculate(1), std::errc::invalid_argument);
    }
}
#endif
