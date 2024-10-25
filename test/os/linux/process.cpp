#include <zero/os/linux/process.h>
#include <zero/os/unix/error.h>
#include <zero/filesystem/fs.h>
#include <zero/filesystem/std.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

TEST_CASE("list process ids", "[linux]") {
    const auto ids = zero::os::linux::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(getpid()));
}

TEST_CASE("self process", "[linux]") {
    using namespace std::chrono_literals;

    const auto currentPath = zero::filesystem::currentPath();
    REQUIRE(currentPath);

    const auto pid = getpid();
    const auto process = zero::os::linux::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

    const auto ppid = process->ppid();
    REQUIRE(ppid);
    REQUIRE(*ppid == getppid());

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

TEST_CASE("child process", "[linux]") {
    using namespace std::chrono_literals;

    const auto currentPath = zero::filesystem::currentPath();
    REQUIRE(currentPath);

    const auto pid = fork();

    if (pid == 0) {
        pause();
        std::exit(EXIT_FAILURE);
    }

    REQUIRE(pid > 0);
    std::this_thread::sleep_for(100ms);

    auto process = zero::os::linux::process::open(pid);
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

TEST_CASE("zombie process", "[linux]") {
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

    const auto process = zero::os::linux::process::open(pid);
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

    const auto comm = process->comm();
    REQUIRE(comm);
    REQUIRE(*comm == "zero_test");

    const auto cmdline = process->cmdline();
    REQUIRE_FALSE(cmdline);
    REQUIRE(cmdline.error() == zero::os::linux::procfs::process::Process::Error::MAYBE_ZOMBIE_PROCESS);

    const auto envs = process->envs();
    REQUIRE((!envs || envs->empty()));

    const auto exe = process->exe();
    REQUIRE_FALSE(exe);
    REQUIRE(exe.error() == std::errc::no_such_file_or_directory);

    const auto cwd = process->cwd();
    REQUIRE_FALSE(cwd);
    REQUIRE(cwd.error() == std::errc::no_such_file_or_directory);

    const auto id = zero::os::unix::ensure([&] {
        return waitpid(pid, nullptr, 0);
    });
    REQUIRE(id);
    REQUIRE(*id == pid);
}

TEST_CASE("open process failed", "[procfs]") {
    const auto process = zero::os::linux::process::open(99999);
    REQUIRE_FALSE(process);
    REQUIRE(process.error() == std::errc::no_such_file_or_directory);
}
