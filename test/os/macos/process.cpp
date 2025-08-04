#include <catch_extensions.h>
#include <zero/os/process.h>
#include <zero/filesystem/fs.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <unistd.h>
#include <csignal>
#include <ranges>

TEST_CASE("list process ids - macOS", "[os::macos::process]") {
    const auto ids = zero::os::macos::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(getpid()));
}

TEST_CASE("process - macOS", "[os::macos::process]") {
    const std::string program{"sleep"};
    constexpr std::array arguments{"1"};

    auto child = zero::os::process::Command{program}
                 .args({arguments.begin(), arguments.end()})
                 .env("ZERO_MACOS_PROCESS_TESTS", "1")
                 .stdInput(zero::os::process::Command::StdioType::NUL)
                 .stdOutput(zero::os::process::Command::StdioType::NUL)
                 .stdError(zero::os::process::Command::StdioType::NUL)
                 .spawn();
    REQUIRE(child);

    auto process = zero::os::macos::process::open(child->pid());
    REQUIRE(process);

    SECTION("pid") {
        REQUIRE(process->pid() == child->pid());
    }

    SECTION("ppid") {
        REQUIRE(process->ppid() == getpid());
    }

    SECTION("comm") {
        REQUIRE(process->comm() == program);
    }

    SECTION("cwd") {
        REQUIRE(process->cwd() == zero::filesystem::currentPath());
    }

    SECTION("exe") {
        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(exe->filename() == program);
    }

    SECTION("cmdline") {
        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::EndsWith(program));
        REQUIRE_THAT(
            (std::ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
            Catch::Matchers::RangeEquals(arguments)
        );
    }

    SECTION("envs") {
        const auto envs = process->envs();
        REQUIRE(envs);
        REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_MACOS_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_MACOS_PROCESS_TESTS") == "1");
    }

    SECTION("start time") {
        using namespace std::chrono_literals;
        const auto startTime = process->startTime();
        REQUIRE(startTime);
        REQUIRE(std::chrono::system_clock::now() - *startTime < 1min);
    }

    SECTION("cpu") {
        const auto cpu = process->cpu();
        REQUIRE(cpu);
    }

    SECTION("memory") {
        const auto memory = process->memory();
        REQUIRE(memory);
    }

    SECTION("io") {
        const auto io = process->io();
        REQUIRE(io);
    }

    SECTION("kill") {
        REQUIRE(process->kill(SIGKILL));
    }

    REQUIRE(child->wait());
}

TEST_CASE("no such process - macOS", "[os::macos::process]") {
    REQUIRE_ERROR(zero::os::macos::process::open(std::numeric_limits<pid_t>::max()), std::errc::no_such_process);
}
