#include <zero/os/darwin/error.h>
#include <catch2/catch_test_macros.hpp>
#include <mach/mach.h>

TEST_CASE("darwin error code", "[error code]") {
    auto ec = make_error_code(static_cast<zero::os::darwin::Error>(KERN_OPERATION_TIMED_OUT));
    REQUIRE(strcmp(ec.category().name(), "zero::os::darwin") == 0);
    REQUIRE(ec == std::errc::timed_out);
    REQUIRE(ec.value() == KERN_OPERATION_TIMED_OUT);
    REQUIRE(ec.message() == mach_error_string(KERN_OPERATION_TIMED_OUT));

    ec = make_error_code(static_cast<zero::os::darwin::Error>(KERN_INVALID_ARGUMENT));
    REQUIRE(strcmp(ec.category().name(), "zero::os::darwin") == 0);
    REQUIRE(ec == std::errc::invalid_argument);
    REQUIRE(ec.value() == KERN_INVALID_ARGUMENT);
    REQUIRE(ec.message() == mach_error_string(KERN_INVALID_ARGUMENT));
}