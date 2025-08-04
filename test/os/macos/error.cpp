#include <catch_extensions.h>
#include <zero/os/macos/error.h>
#include <mach/mach.h>

TEST_CASE("macos kernel errors", "[os::macos::error]") {
    using namespace std::string_view_literals;

    SECTION("timeout") {
        const std::error_code ec{static_cast<zero::os::macos::Error>(KERN_OPERATION_TIMED_OUT)};
        REQUIRE(ec == std::errc::timed_out);
        REQUIRE(ec.message() == mach_error_string(KERN_OPERATION_TIMED_OUT));
    }

    SECTION("invalid argument") {
        const std::error_code ec{static_cast<zero::os::macos::Error>(KERN_INVALID_ARGUMENT)};
        REQUIRE(ec == std::errc::invalid_argument);
        REQUIRE(ec.message() == mach_error_string(KERN_INVALID_ARGUMENT));
    }
}
