#include <zero/formatter.h>
#include <catch2/catch_test_macros.hpp>
#include <fmt/std.h>

TEST_CASE("format expected", "[formatter]") {
    SECTION("void") {
        SECTION("has value") {
            REQUIRE(fmt::to_string(tl::expected<void, std::error_code>{}) == "expected()");
        }

        SECTION("has error") {
            REQUIRE(
                fmt::to_string(
                    tl::expected<void, std::error_code>{tl::unexpected{make_error_code(std::errc::io_error)}}
                )
                == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::io_error))
            );
        }
    }

    SECTION("not void") {
        SECTION("has value") {
            REQUIRE(fmt::to_string(tl::expected<int, std::error_code>{1}) == "expected(1)");
        }

        SECTION("has error") {
            REQUIRE(
                fmt::to_string(
                    tl::expected<int, std::error_code>{tl::unexpected{make_error_code(std::errc::io_error)}}
                )
                == fmt::format("unexpected(generic:{})", static_cast<int>(std::errc::io_error))
            );
        }
    }
}

TEST_CASE("format exception pointer", "[formatter]") {
    SECTION("null") {
        REQUIRE(fmt::to_string(std::exception_ptr{}) == "nullptr");
    }

    SECTION("not null") {
        REQUIRE(fmt::to_string(std::make_exception_ptr(std::runtime_error{"hello world"})) == "exception(hello world)");
    }
}
