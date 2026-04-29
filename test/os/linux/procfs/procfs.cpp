#include <catch_extensions.h>
#include <zero/os/linux/procfs/procfs.h>

TEST_CASE("system stat", "[os::linux::procfs]") {
    const auto stat = zero::os::linux::procfs::stat();
    REQUIRE(stat.bootTime > 0);
    REQUIRE(!stat.cpuTimes.empty());
    REQUIRE(stat.processesCreated > 0);
}

TEST_CASE("memory stat", "[os::linux::procfs]") {
    const auto memory = zero::os::linux::procfs::memory();
    REQUIRE(memory.memoryTotal > 0);
    REQUIRE(memory.memoryFree <= memory.memoryTotal);
}
