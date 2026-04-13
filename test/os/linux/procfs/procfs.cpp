#include <catch_extensions.h>
#include <zero/os/linux/procfs/procfs.h>

TEST_CASE("stat", "[os::linux::procfs]") {
    REQUIRE_NOTHROW(zero::os::linux::procfs::stat());
}

TEST_CASE("memory stat", "[os::linux::procfs]") {
    REQUIRE_NOTHROW(zero::os::linux::procfs::memory());
}
