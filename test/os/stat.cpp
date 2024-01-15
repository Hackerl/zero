#include <zero/os/stat.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("system stat", "[os]") {
    const auto cpu = zero::os::stat::cpu();
    REQUIRE(cpu);

    const auto memory = zero::os::stat::memory();
    REQUIRE(memory);
}
