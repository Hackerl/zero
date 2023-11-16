#include <zero/os/darwin/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

using namespace std::chrono_literals;

TEST_CASE("darwin process", "[process]") {
    SECTION("all") {
        const auto ids = zero::os::darwin::process::all();
        REQUIRE(ids);
    }

    SECTION("self") {
        const auto pid = getpid();
        const auto process = zero::os::darwin::process::self();
        REQUIRE(process);
        REQUIRE(process->pid() == pid);
        REQUIRE(process->ppid() == getppid());

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto name = process->name();
        REQUIRE(name);
        REQUIRE(*name == "zero_test");

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "zero_test");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        const auto env = process->env();
        REQUIRE(env);

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        const auto memory = process->memory();
        REQUIRE(memory);

        const auto cpu = process->cpu();
        REQUIRE(cpu);

        const auto io = process->io();
        REQUIRE(io);
    }

    SECTION("child") {
        const pid_t pid = fork();

        if (pid == 0) {
            pause();
            exit(0);
        }

        REQUIRE(pid > 0);
        std::this_thread::sleep_for(100ms);

        const auto process = zero::os::darwin::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "zero_test");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        const auto env = process->env();
        REQUIRE(env);

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        const auto memory = process->memory();
        REQUIRE(memory);

        const auto cpu = process->cpu();
        REQUIRE(cpu);

        const auto io = process->io();
        REQUIRE(io);

        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }

    SECTION("zombie") {
        const pid_t pid = fork();

        if (pid == 0) {
            pause();
            exit(0);
        }

        REQUIRE(pid > 0);

        kill(pid, SIGKILL);
        std::this_thread::sleep_for(100ms);

        const auto process = zero::os::darwin::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(!comm);
        REQUIRE(comm.error() == std::errc::no_such_process);

        const auto cmdline = process->cmdline();
        REQUIRE(!cmdline);
        REQUIRE(cmdline.error() == std::errc::invalid_argument);

        const auto env = process->env();
        REQUIRE(!env);
        REQUIRE(env.error() == std::errc::invalid_argument);

        const auto exe = process->exe();
        REQUIRE(!exe);
        REQUIRE(exe.error() == std::errc::no_such_process);

        const auto cwd = process->cwd();
        REQUIRE(!cwd);
        REQUIRE(cwd.error() == std::errc::no_such_process);

        waitpid(pid, nullptr, 0);
    }

    SECTION("no such process") {
        const auto process = zero::os::darwin::process::open(99999);
        REQUIRE(!process);
        REQUIRE(process.error() == std::errc::no_such_process);
    }
}