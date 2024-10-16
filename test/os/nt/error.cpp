#include <zero/os/nt/error.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("nt error", "[nt]") {
    std::error_code ec{static_cast<zero::os::nt::ResultHandle>(E_ACCESSDENIED)};
    REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    REQUIRE(ec == std::errc::permission_denied);

    ec = static_cast<zero::os::nt::ResultHandle>(E_OUTOFMEMORY);
    REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    REQUIRE(ec == std::errc::not_enough_memory);

    ec = static_cast<zero::os::nt::ResultHandle>(E_INVALIDARG);
    REQUIRE_THAT(ec.message(), !Catch::Matchers::StartsWith("unknown HRESULT"));
    REQUIRE(ec == std::errc::invalid_argument);
}
