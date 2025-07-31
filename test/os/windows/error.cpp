#include <catch_extensions.h>
#include <zero/os/windows/error.h>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("result handle", "[os::windows::error]") {
    using namespace std::string_view_literals;

    SECTION("access denied") {
        const std::error_code ec{static_cast<zero::os::windows::ResultHandle>(E_ACCESSDENIED)};
        REQUIRE(ec.category().name() == "HRESULT"sv);
        REQUIRE(ec == std::errc::permission_denied);
        REQUIRE(ec.value() == E_ACCESSDENIED);
        REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    }

    SECTION("out of memory") {
        const std::error_code ec{static_cast<zero::os::windows::ResultHandle>(E_OUTOFMEMORY)};
        REQUIRE(ec.category().name() == "HRESULT"sv);
        REQUIRE(ec == std::errc::not_enough_memory);
        REQUIRE(ec.value() == E_OUTOFMEMORY);
        REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    }

    SECTION("invalid argument") {
        const std::error_code ec{static_cast<zero::os::windows::ResultHandle>(E_INVALIDARG)};
        REQUIRE(ec.category().name() == "HRESULT"sv);
        REQUIRE(ec == std::errc::invalid_argument);
        REQUIRE(ec.value() == E_INVALIDARG);
        REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    }
}
