#include <zero/os/nt/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("windows process", "[nt]") {
    const auto ids = zero::os::nt::process::all();
    REQUIRE(ids);

    const auto process = zero::os::nt::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == GetCurrentProcessId());

    const auto path = zero::filesystem::getApplicationPath();
    REQUIRE(path);

    const auto name = process->name();
    REQUIRE(name);
    REQUIRE(*name == path->filename());

    const auto exe = process->exe();
    REQUIRE(exe);
    REQUIRE(*exe == *path);

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE(cmdline->at(0).find(path->filename().string()) != std::string::npos);

    const auto cwd = process->cwd();
    REQUIRE(cwd);
    REQUIRE(*cwd == std::filesystem::current_path());

    const auto envs = process->envs();
    REQUIRE(envs);

    const auto memory = process->memory();
    REQUIRE(memory);

    const auto cpu = process->cpu();
    REQUIRE(cpu);

    const auto io = process->io();
    REQUIRE(io);

    const auto code = process->exitCode();
    REQUIRE(!code);
    REQUIRE(code.error() == zero::os::nt::process::Error::PROCESS_STILL_ACTIVE);

    auto result = process->wait(10ms);
    REQUIRE(!result);
    REQUIRE(result.error() == std::errc::timed_out);

    result = process->tryWait();
    REQUIRE(!result);
    REQUIRE(result.error() == std::errc::operation_would_block);
}
