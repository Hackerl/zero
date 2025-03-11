#include <catch_extensions.h>
#include <zero/os/linux/procfs/procfs.h>

TEST_CASE("stat", "[os::linux::procfs]") {
    const auto stat = zero::os::linux::procfs::stat();
    REQUIRE(stat);
}

TEST_CASE("memory stat", "[os::linux::procfs]") {
    const auto memory = zero::os::linux::procfs::memory();
    REQUIRE(memory);
}
