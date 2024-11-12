#include <zero/os/linux/procfs/procfs.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("stat", "[procfs]") {
    const auto stat = zero::os::linux::procfs::stat();
    REQUIRE(stat);
}

TEST_CASE("memory stat", "[procfs]") {
    const auto memory = zero::os::linux::procfs::memory();
    REQUIRE(memory);
}
