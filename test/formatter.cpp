#include <zero/formatter.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("custom type formatter", "[formatter]") {
    SECTION("expected") {
        std::expected<void, std::error_code> e1;
        REQUIRE(fmt::to_string(e1) == "expected()");

        e1 = std::unexpected(make_error_code(std::errc::io_error));
        REQUIRE(fmt::to_string(e1) == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::io_error)));

        std::expected<int, std::error_code> e2 = 1;
        REQUIRE(fmt::to_string(e2) == "expected(1)");

        e2 = std::unexpected(make_error_code(std::errc::io_error));
        REQUIRE(fmt::to_string(e2) == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::io_error)));

        std::expected<std::string, std::error_code> e3 = "hello world";
        REQUIRE(fmt::to_string(e3) == "expected(hello world)");

        e3 = std::unexpected(make_error_code(std::errc::io_error));
        REQUIRE(fmt::to_string(e3) == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::io_error)));
    }

    SECTION("exception pointer") {
        std::exception_ptr ptr;
        REQUIRE(fmt::to_string(ptr) == "nullptr");

        ptr = std::make_exception_ptr(std::runtime_error("hello world"));
        REQUIRE(fmt::to_string(ptr) == "exception(hello world)");
    }
}
