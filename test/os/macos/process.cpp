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

TEST_CASE("list process ids", "[macos]") {
    const auto ids = zero::os::macos::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(getpid()));
}

TEST_CASE("self process", "[macos]") {
    using namespace std::chrono_literals;

    const auto currentPath = zero::filesystem::currentPath();
    REQUIRE(currentPath);

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

    const auto startTime = process->startTime();
    REQUIRE(startTime);
    REQUIRE(std::chrono::system_clock::now() - *startTime < 1min);

    const auto memory = process->memory();
    REQUIRE(memory);

    const auto cpu = process->cpu();
    REQUIRE(cpu);

    const auto io = process->io();
    REQUIRE(io);
}

TEST_CASE("child process", "[macos]") {
    using namespace std::chrono_literals;

    const auto currentPath = zero::filesystem::currentPath();
    REQUIRE(currentPath);

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

TEST_CASE("zombie process", "[macos]") {
    using namespace std::chrono_literals;

    const auto pid = fork();

    if (pid == 0) {
        pause();
        std::exit(EXIT_FAILURE);
    }

    REQUIRE(pid > 0);
    REQUIRE(zero::os::unix::expected([=] {
        return kill(pid, SIGKILL);
    }));

    std::this_thread::sleep_for(100ms);

    const auto process = zero::os::macos::process::open(pid);
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

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

TEST_CASE("open process failed", "macos") {
    const auto process = zero::os::macos::process::open(99999);
    REQUIRE_FALSE(process);
    REQUIRE(process.error() == std::errc::no_such_process);
}
