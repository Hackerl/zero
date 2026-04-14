#include <catch_extensions.h>
#include <zero/os/stat.h>

TEST_CASE("system cpu stat", "[os::stat]") {
    const auto [user, system, idle] = zero::os::stat::cpu();
    REQUIRE(user >= 0);
    REQUIRE(system >= 0);
    REQUIRE(idle >= 0);
}

TEST_CASE("system memory stat", "[os::stat]") {
    const auto [total, used, available, free, usedPercent] = zero::os::stat::memory();
    REQUIRE(total > 0);
    REQUIRE(used <= total);
    REQUIRE(available <= total);
    REQUIRE(free <= total);
    REQUIRE(usedPercent >= 0.0);
    REQUIRE(usedPercent <= 100.0);
}
