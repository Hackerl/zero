#include <zero/os/macos/process.h>
#include <zero/os/unix/error.h>
#include <zero/filesystem/fs.h>
#include <zero/filesystem/std.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

TEST_CASE("macos process", "[macos]") {
    SECTION("all") {
        const auto ids = zero::os::macos::process::all();
        REQUIRE(ids);
    }

    const auto currentPath = zero::filesystem::currentPath();
    REQUIRE(currentPath);

    SECTION("self") {
        const auto pid = getpid();
        const auto process = zero::os::macos::process::self();
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto ppid = process->ppid();
        REQUIRE(ppid);
        REQUIRE(*ppid == getppid());

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto name = process->name();
        REQUIRE(name);
        REQUIRE(*name == "zero_test");

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "zero_test");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

        const auto envs = process->envs();
        REQUIRE(envs);

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == *currentPath);

        const auto memory = process->memory();
        REQUIRE(memory);

        const auto cpu = process->cpu();
        REQUIRE(cpu);

        const auto io = process->io();
        REQUIRE(io);
    }

    SECTION("child") {
        using namespace std::chrono_literals;

        const auto pid = fork();

        if (pid == 0) {
            pause();
            std::exit(EXIT_FAILURE);
        }

        REQUIRE(pid > 0);
        std::this_thread::sleep_for(100ms);

        auto process = zero::os::macos::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "zero_test");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

        const auto envs = process->envs();
        REQUIRE(envs);

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == *currentPath);

        const auto memory = process->memory();
        REQUIRE(memory);

        const auto cpu = process->cpu();
        REQUIRE(cpu);

        const auto io = process->io();
        REQUIRE(io);

        REQUIRE(process->kill(SIGKILL));

        const auto id = zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        });
        REQUIRE(id);
        REQUIRE(*id == pid);
    }

    SECTION("zombie") {
        using namespace std::chrono_literals;

        const auto pid = fork();

        if (pid == 0) {
            pause();
            std::exit(EXIT_FAILURE);
        }

        REQUIRE(pid > 0);

        kill(pid, SIGKILL);
        std::this_thread::sleep_for(100ms);

        const auto process = zero::os::macos::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE_FALSE(comm);
        REQUIRE(comm.error() == std::errc::no_such_process);

        const auto cmdline = process->cmdline();
        REQUIRE_FALSE(cmdline);
        REQUIRE(cmdline.error() == std::errc::invalid_argument);

        const auto envs = process->envs();
        REQUIRE_FALSE(envs);
        REQUIRE(envs.error() == std::errc::invalid_argument);

        const auto exe = process->exe();
        REQUIRE_FALSE(exe);
        REQUIRE(exe.error() == std::errc::no_such_process);

        const auto cwd = process->cwd();
        REQUIRE_FALSE(cwd);
        REQUIRE(cwd.error() == std::errc::no_such_process);

        const auto id = zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        });
        REQUIRE(id);
        REQUIRE(*id == pid);
    }

    SECTION("no such process") {
        const auto process = zero::os::macos::process::open(99999);
        REQUIRE_FALSE(process);
        REQUIRE(process.error() == std::errc::no_such_process);
    }
}