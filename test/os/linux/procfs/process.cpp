#include <catch_extensions.h>
#include <zero/os/process.h>
#include <zero/filesystem/fs.h>
#include <zero/os/unix/error.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <unistd.h>

TEST_CASE("list process ids - procfs", "[os::linux::procfs::process]") {
    const auto ids = zero::os::linux::procfs::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(getpid()));
}

TEST_CASE("process - procfs", "[os::linux::procfs::process]") {
    constexpr auto program{"sleep"};
    constexpr std::array arguments{"1"};

    auto child = zero::os::process::Command{program}
                 .args({arguments.begin(), arguments.end()})
                 .env("ZERO_LINUX_PROCFS_PROCESS_TESTS", "1")
                 .stdInput(zero::os::process::Command::StdioType::NUL)
                 .stdOutput(zero::os::process::Command::StdioType::NUL)
                 .stdError(zero::os::process::Command::StdioType::NUL)
                 .spawn();
    REQUIRE(child);

    auto process = zero::os::linux::procfs::process::open(static_cast<pid_t>(child->pid()));
    REQUIRE(process);

    SECTION("pid") {
        REQUIRE(process->pid() == child->pid());
    }

    SECTION("exe") {
        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(exe->filename() == program);
    }

    SECTION("cwd") {
        REQUIRE(process->cwd() == zero::filesystem::currentPath());
    }

    SECTION("comm") {
        REQUIRE(process->comm() == program);
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

    SECTION("environ") {
        const auto envs = process->environ();
        REQUIRE(envs);
        REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_LINUX_PROCFS_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_LINUX_PROCFS_PROCESS_TESTS") == "1");
    }

    SECTION("stat") {
        const auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == child->pid());
        REQUIRE(stat->comm == program);
        REQUIRE(stat->ppid == getpid());
    }

    SECTION("statM") {
        const auto statM = process->statM();
        REQUIRE(statM);
    }

    SECTION("status") {
        const auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->pid == child->pid());
        REQUIRE(status->name == program);
        REQUIRE(status->ppid == getpid());
    }

    SECTION("tasks") {
        const auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE_THAT(*tasks, Catch::Matchers::Contains(child->pid()));
    }

    SECTION("maps") {
        const auto mappings = process->maps();
        REQUIRE(mappings);
    }

    SECTION("io") {
        const auto io = process->io();
        REQUIRE(io);
    }

    REQUIRE(child->wait());
}

TEST_CASE("no such process - procfs", "[os::linux::procfs::process]") {
    REQUIRE_ERROR(
        zero::os::linux::procfs::process::open(std::numeric_limits<pid_t>::max()),
        std::errc::no_such_file_or_directory
    );
}
