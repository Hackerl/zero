#include <zero/os/nt/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("windows process", "[process]") {
    DWORD pid = GetCurrentProcessId();
    auto process = zero::os::nt::process::open(pid);
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

    auto path = zero::filesystem::getApplicationPath();
    REQUIRE(path);

    auto name = process->name();
    REQUIRE(name);
    REQUIRE(*name == path->filename());

    auto image = process->image();
    REQUIRE(image);
    REQUIRE(*image == *path);

    auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE(cmdline->at(0).find(path->filename().string()) != std::string::npos);
}