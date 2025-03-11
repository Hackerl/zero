#include <catch_extensions.h>
#include <zero/os/linux/process.h>
#include <zero/os/unix/error.h>
#include <zero/filesystem/fs.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

TEST_CASE("list process ids", "[os::linux::process]") {
    const auto ids = zero::os::linux::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(getpid()));
}

TEST_CASE("self process", "[os::linux::process]") {
    using namespace std::chrono_literals;

    const auto pid = getpid();
    const auto process = zero::os::linux::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == pid);
    REQUIRE(process->ppid() == getppid());

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

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
    REQUIRE(std::chrono::system_clock::now() - *startTime < 10min);

    const auto memory = process->memory();
    REQUIRE(memory);

    const auto cpu = process->cpu();
    REQUIRE(cpu);

    const auto io = process->io();
    REQUIRE(io);
}

TEST_CASE("child process", "[os::linux::process]") {
    using namespace std::chrono_literals;

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

TEST_CASE("zombie process", "[os::linux::process]") {
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

    REQUIRE(process->comm() == "zero_test");
    REQUIRE_ERROR(process->cmdline(), zero::os::linux::procfs::process::Process::Error::MAYBE_ZOMBIE_PROCESS);

    const auto envs = process->envs();
    REQUIRE((!envs || envs->empty()));

    REQUIRE_ERROR(process->exe(), std::errc::no_such_file_or_directory);
    REQUIRE_ERROR(process->cwd(), std::errc::no_such_file_or_directory);

    REQUIRE(
        zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }) == pid
    );
}

TEST_CASE("open process failed", "[os::linux::process]") {
    REQUIRE_ERROR(zero::os::linux::process::open(99999), std::errc::no_such_file_or_directory);
}
