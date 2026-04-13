#include <catch_extensions.h>
#include <zero/os/stat.h>

TEST_CASE("system cpu stat", "[os::stat]") {
    REQUIRE_NOTHROW(zero::os::stat::cpu());
}

TEST_CASE("system memory stat", "[os::stat]") {
    REQUIRE_NOTHROW(zero::os::stat::memory());
}
