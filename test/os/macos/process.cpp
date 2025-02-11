#include <catch_extensions.h>
#include <zero/os/macos/process.h>
#include <zero/os/unix/error.h>
#include <zero/filesystem/fs.h>
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

    const auto pid = getpid();
    const auto process = zero::os::macos::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == pid);
    REQUIRE(process->ppid() == getppid());

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

    REQUIRE(process->name() == "zero_test");
    REQUIRE(process->comm() == "zero_test");

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

    const auto envs = process->envs();
    REQUIRE(envs);

    REQUIRE(process->exe() == *path);
    REQUIRE(process->cwd() == zero::filesystem::currentPath());

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

    REQUIRE(process->name() == "zero_test");
    REQUIRE(process->comm() == "zero_test");

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

    const auto envs = process->envs();
    REQUIRE(envs);

    REQUIRE(process->exe() == *path);
    REQUIRE(process->cwd() == zero::filesystem::currentPath());

    const auto memory = process->memory();
    REQUIRE(memory);

    const auto cpu = process->cpu();
    REQUIRE(cpu);

    const auto io = process->io();
    REQUIRE(io);

    REQUIRE(process->kill(SIGKILL));

    REQUIRE(
        zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }) == pid
    );
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

    REQUIRE_ERROR(process->name(), std::errc::no_such_process);
    REQUIRE_ERROR(process->comm(), std::errc::no_such_process);
    REQUIRE_ERROR(process->cmdline(), std::errc::invalid_argument);
    REQUIRE_ERROR(process->envs(), std::errc::invalid_argument);
    REQUIRE_ERROR(process->exe(), std::errc::no_such_process);
    REQUIRE_ERROR(process->cwd(), std::errc::no_such_process);

    REQUIRE(
        zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }) == pid
    );
}

TEST_CASE("open process failed", "macos") {
    REQUIRE_ERROR(zero::os::macos::process::open(99999), std::errc::no_such_process);
}
