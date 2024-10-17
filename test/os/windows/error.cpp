#include <zero/os/windows/error.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("windows error", "[windows]") {
    std::error_code ec{static_cast<zero::os::windows::ResultHandle>(E_ACCESSDENIED)};
    REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    REQUIRE(ec == std::errc::permission_denied);

    ec = static_cast<zero::os::windows::ResultHandle>(E_OUTOFMEMORY);
    REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    REQUIRE(ec == std::errc::not_enough_memory);

    ec = static_cast<zero::os::windows::ResultHandle>(E_INVALIDARG);
    REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    REQUIRE(ec == std::errc::invalid_argument);
}
