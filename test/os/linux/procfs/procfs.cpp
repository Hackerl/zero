#include <zero/os/linux/procfs/procfs.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("procfs", "[procfs]") {
    SECTION("stat") {
        const auto stat = zero::os::linux::procfs::stat();
        REQUIRE(stat);
    }

    SECTION("memory") {
        const auto memory = zero::os::linux::procfs::memory();
        REQUIRE(memory);
    }
}
