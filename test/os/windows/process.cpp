#include <zero/os/windows/process.h>
#include <zero/filesystem/fs.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("windows process", "[windows]") {
    using namespace std::chrono_literals;

    const auto ids = zero::os::windows::process::all();
    REQUIRE(ids);

    const auto process = zero::os::windows::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == GetCurrentProcessId());

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

    const auto name = process->name();
    REQUIRE(name);
    REQUIRE(*name == path->filename());

    const auto exe = process->exe();
    REQUIRE(exe);
    REQUIRE(*exe == *path);

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

    const auto cwd = process->cwd();
    REQUIRE(cwd);
    REQUIRE(*cwd == std::filesystem::current_path());

    const auto envs = process->envs();
    REQUIRE(envs);

    const auto startTime = process->startTime();
    REQUIRE(startTime);
    REQUIRE(std::chrono::system_clock::now() - *startTime < 1min);

    const auto memory = process->memory();
    REQUIRE(memory);

    const auto cpu = process->cpu();
    REQUIRE(cpu);

    const auto io = process->io();
    REQUIRE(io);

    const auto code = process->exitCode();
    REQUIRE_FALSE(code);
    REQUIRE(code.error() == zero::os::windows::process::Process::Error::PROCESS_STILL_ACTIVE);

    const auto result = process->wait(10ms);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == std::errc::timed_out);
}
