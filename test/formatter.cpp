#include "catch_extensions.h"
#include <zero/formatter.h>

TEST_CASE("std::exception_ptr formatter", "[formatter]") {
    SECTION("null") {
        REQUIRE(fmt::to_string(std::exception_ptr{}) == "nullptr");
    }

    SECTION("not null") {
        REQUIRE(fmt::to_string(std::make_exception_ptr(std::runtime_error{"hello world"})) == "exception(hello world)");
    }
}
