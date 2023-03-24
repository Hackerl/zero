#include <zero/os/process.h>
#include <catch2/catch_test_macros.hpp>
#include <process.h>

TEST_CASE("windows process", "[process]") {
    std::optional<zero::os::process::Process> process = zero::os::process::openProcess(_getpid());

    REQUIRE(process);
    REQUIRE(process->name());
    REQUIRE(process->image());
    REQUIRE(process->cmdline());
}