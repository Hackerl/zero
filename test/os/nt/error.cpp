#include <zero/os/nt/error.h>
#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("nt error", "[nt]") {
    std::error_code ec = static_cast<zero::os::nt::ResultHandle>(E_ACCESSDENIED);
    REQUIRE(!zero::strings::startsWith(ec.message(), "unknown HRESULT"));
    REQUIRE(ec == std::errc::permission_denied);

    ec = static_cast<zero::os::nt::ResultHandle>(E_OUTOFMEMORY);
    REQUIRE(!zero::strings::startsWith(ec.message(), "unknown HRESULT"));
    REQUIRE(ec == std::errc::not_enough_memory);

    ec = static_cast<zero::os::nt::ResultHandle>(E_INVALIDARG);
    REQUIRE(!zero::strings::startsWith(ec.message(), "unknown HRESULT"));
    REQUIRE(ec == std::errc::invalid_argument);
}
