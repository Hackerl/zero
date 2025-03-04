#include "catch_extensions.h"
#include <zero/formatter.h>
#include <fmt/std.h>

TEST_CASE("std::expected formatter", "[formatter]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(fmt::to_string(std::expected<void, std::error_code>{}) == "expected()");
        }

        SECTION("has error") {
            REQUIRE(
                fmt::to_string(
                    std::expected<void, std::error_code>{std::unexpected{make_error_code(std::errc::io_error)}}
                )
                == fmt::format("unexpected(generic:{})", std::to_underlying(std::errc::io_error))
            );
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(fmt::to_string(std::expected<int, std::error_code>{1}) == "expected(1)");
        }

        SECTION("has error") {
            REQUIRE(
                fmt::to_string(
                    std::expected<int, std::error_code>{std::unexpected{make_error_code(std::errc::io_error)}}
                )
                == fmt::format("unexpected(generic:{})", std::to_underlying(std::errc::io_error))
            );
        }
    }
}

TEST_CASE("std::exception_ptr formatter", "[formatter]") {
    SECTION("null") {
        REQUIRE(fmt::to_string(std::exception_ptr{}) == "nullptr");
    }

    SECTION("not null") {
        REQUIRE(fmt::to_string(std::make_exception_ptr(std::runtime_error{"hello world"})) == "exception(hello world)");
    }
}
