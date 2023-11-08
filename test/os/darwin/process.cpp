#include <zero/os/darwin/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

using namespace std::chrono_literals;

TEST_CASE("darwin process", "[process]") {
    SECTION("self") {
        auto pid = getpid();
        auto process = zero::os::darwin::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);
        REQUIRE(process->ppid() == getppid());

        auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        auto name = process->name();
        REQUIRE(name);
        REQUIRE(*name == "zero_test");

        auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "zero_test");

        auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        auto env = process->env();
        REQUIRE(env);

        auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        auto memory = process->memory();
        REQUIRE(memory);

        auto cpu = process->cpu();
        REQUIRE(cpu);
    }

    SECTION("child") {
        pid_t pid = fork();

        if (pid == 0) {
            pause();
            exit(0);
        }

        REQUIRE(pid > 0);
        std::this_thread::sleep_for(100ms);

        auto process = zero::os::darwin::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "zero_test");

        auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        auto env = process->env();
        REQUIRE(env);

        auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }

    SECTION("zombie") {
        pid_t pid = fork();

        if (pid == 0) {
            pause();
            exit(0);
        }

        REQUIRE(pid > 0);

        kill(pid, SIGKILL);
        std::this_thread::sleep_for(100ms);

        auto process = zero::os::darwin::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        auto comm = process->comm();
        REQUIRE(!comm);
        REQUIRE(comm.error() == std::errc::no_such_process);

        auto cmdline = process->cmdline();
        REQUIRE(!cmdline);
        REQUIRE(cmdline.error() == std::errc::invalid_argument);

        auto env = process->env();
        REQUIRE(!env);
        REQUIRE(env.error() == std::errc::invalid_argument);

        auto exe = process->exe();
        REQUIRE(!exe);
        REQUIRE(exe.error() == std::errc::no_such_process);

        auto cwd = process->cwd();
        REQUIRE(!cwd);
        REQUIRE(cwd.error() == std::errc::no_such_process);

        waitpid(pid, nullptr, 0);
    }

    SECTION("no such process") {
        auto process = zero::os::darwin::process::open(99999);
        REQUIRE(!process);
        REQUIRE(process.error() == std::errc::no_such_process);
    }
}