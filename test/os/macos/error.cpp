#include <zero/os/macos/error.h>
#include <catch2/catch_test_macros.hpp>
#include <mach/mach.h>

TEST_CASE("macos kernel error", "[macos]") {
    using namespace std::string_view_literals;

    std::error_code ec{static_cast<zero::os::macos::Error>(KERN_OPERATION_TIMED_OUT)};
    REQUIRE(ec.category().name() == "zero::os::macos"sv);
    REQUIRE(ec == std::errc::timed_out);
    REQUIRE(ec.value() == KERN_OPERATION_TIMED_OUT);
    REQUIRE(ec.message() == mach_error_string(KERN_OPERATION_TIMED_OUT));

    ec = static_cast<zero::os::macos::Error>(KERN_INVALID_ARGUMENT);
    REQUIRE(ec.category().name() == "zero::os::macos"sv);
    REQUIRE(ec == std::errc::invalid_argument);
    REQUIRE(ec.value() == KERN_INVALID_ARGUMENT);
    REQUIRE(ec.message() == mach_error_string(KERN_INVALID_ARGUMENT));
}
