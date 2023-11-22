#include <zero/formatter.h>
#include <catch2/catch_test_macros.hpp>
#include <fmt/std.h>

TEST_CASE("custom type formatter", "[formatter]") {
    tl::expected<void, std::error_code> e1;
    REQUIRE(fmt::to_string(e1) == "expected()");

    e1 = tl::unexpected(make_error_code(std::errc::invalid_argument));
    REQUIRE(fmt::to_string(e1) == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::invalid_argument)));

    tl::expected<int, std::error_code> e2 = 1;
    REQUIRE(fmt::to_string(e2) == "expected(1)");

    e2 = tl::unexpected(make_error_code(std::errc::invalid_argument));
    REQUIRE(fmt::to_string(e2) == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::invalid_argument)));

    tl::expected<std::string, std::error_code> e3 = "hello world";
    REQUIRE(fmt::to_string(e3) == "expected(hello world)");

    e3 = tl::unexpected(make_error_code(std::errc::invalid_argument));
    REQUIRE(fmt::to_string(e3) == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::invalid_argument)));
}