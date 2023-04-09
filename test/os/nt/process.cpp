#include <zero/os/nt/process.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("windows process", "[process]") {
    std::optional<zero::os::nt::process::Process> process = zero::os::nt::process::openProcess(GetCurrentProcessId());

    REQUIRE(process);
    REQUIRE(process->ppid());
    REQUIRE(process->name());
    REQUIRE(process->image());
    REQUIRE(process->cmdline());
}