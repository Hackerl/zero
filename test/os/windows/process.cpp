#include <catch_extensions.h>
#include <zero/os/windows/process.h>
#include <zero/filesystem/fs.h>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("list process ids", "[windows]") {
    const auto ids = zero::os::windows::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(GetCurrentProcessId()));
}

TEST_CASE("process", "[windows]") {
    using namespace std::chrono_literals;

    const auto process = zero::os::windows::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == GetCurrentProcessId());

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

    REQUIRE(process->name() == path->filename());
    REQUIRE(process->exe() == *path);

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

    REQUIRE(process->cwd() == zero::filesystem::currentPath());

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

    REQUIRE_ERROR(process->exitCode(), zero::os::windows::process::Process::Error::PROCESS_STILL_ACTIVE);
    REQUIRE_ERROR(process->wait(10ms), std::errc::timed_out);
}
