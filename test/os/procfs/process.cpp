#include <zero/os/procfs/process.h>
#include <zero/os/procfs/procfs.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <ranges>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

using namespace std::chrono_literals;

TEST_CASE("procfs process", "[procfs]") {
    char name[16] = {};
    prctl(PR_GET_NAME, name);
    prctl(PR_SET_NAME, "(test)");

    const auto variable = reinterpret_cast<std::uintptr_t>(stdout);
    const auto function = reinterpret_cast<std::uintptr_t>(zero::filesystem::getApplicationPath);

    SECTION("all") {
        const auto ids = zero::os::procfs::process::all();
        REQUIRE(ids);
    }

    SECTION("self") {
        const auto pid = getpid();
        const auto process = zero::os::procfs::process::self();
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        const auto env = process->environ();
        REQUIRE(env);

        const auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::ranges::find_if(
            *mappings,
            [=](const zero::os::procfs::process::MemoryMapping &mapping) {
                return function >= mapping.start && function < mapping.end;
            }
        );
        REQUIRE(it != mappings->end());

        auto permissions = zero::os::procfs::process::MemoryPermission::READ |
            zero::os::procfs::process::MemoryPermission::EXECUTE |
            zero::os::procfs::process::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        it = std::ranges::find_if(
            *mappings,
            [=](const zero::os::procfs::process::MemoryMapping &mapping) {
                return variable >= mapping.start && variable < mapping.end;
            }
        );
        REQUIRE(it != mappings->end());

        permissions = zero::os::procfs::process::MemoryPermission::READ |
            zero::os::procfs::process::MemoryPermission::WRITE |
            zero::os::procfs::process::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        const auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == pid);
        REQUIRE(stat->comm == "(test)");
        REQUIRE(stat->state == 'R');
        REQUIRE(stat->ppid == getppid());
        REQUIRE(stat->pgrp == getpgrp());
        REQUIRE(stat->session == getsid(pid));

        const auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->name == "(test)");
        REQUIRE(status->state == "R (running)");
        REQUIRE(status->tgid == pid);
        REQUIRE(status->pid == pid);
        REQUIRE(status->ppid == getppid());

        const auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE(tasks->size() == 1);
        REQUIRE(tasks->front() == pid);

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

        const auto process = zero::os::procfs::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        const auto env = process->environ();
        REQUIRE(env);

        const auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::ranges::find_if(
            *mappings,
            [=](const zero::os::procfs::process::MemoryMapping &mapping) {
                return function >= mapping.start && function < mapping.end;
            }
        );
        REQUIRE(it != mappings->end());

        auto permissions = zero::os::procfs::process::MemoryPermission::READ |
            zero::os::procfs::process::MemoryPermission::EXECUTE |
            zero::os::procfs::process::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        it = std::ranges::find_if(
            *mappings,
            [=](const zero::os::procfs::process::MemoryMapping &mapping) {
                return variable >= mapping.start && variable < mapping.end;
            }
        );
        REQUIRE(it != mappings->end());

        permissions = zero::os::procfs::process::MemoryPermission::READ |
            zero::os::procfs::process::MemoryPermission::WRITE |
            zero::os::procfs::process::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        const auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == pid);
        REQUIRE(stat->comm == "(test)");
        REQUIRE(stat->state == 'S');
        REQUIRE(stat->ppid == getpid());
        REQUIRE(stat->pgrp == getpgrp());
        REQUIRE(stat->session == getsid(pid));

        const auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->name == "(test)");
        REQUIRE(status->state == "S (sleeping)");
        REQUIRE(status->tgid == pid);
        REQUIRE(status->pid == pid);
        REQUIRE(status->ppid == getpid());

        const auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE(tasks->size() == 1);
        REQUIRE(tasks->front() == pid);

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

        const auto process = zero::os::procfs::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        const auto cmdline = process->cmdline();
        REQUIRE(!cmdline);
        REQUIRE(cmdline.error() == zero::os::procfs::Error::MAYBE_ZOMBIE_PROCESS);

        const auto env = process->environ();
        REQUIRE((!env || env->empty()));

        const auto mappings = process->maps();
        REQUIRE(!mappings);
        REQUIRE(mappings.error() == zero::os::procfs::Error::MAYBE_ZOMBIE_PROCESS);

        const auto exe = process->exe();
        REQUIRE(!exe);
        REQUIRE(exe.error() == std::errc::no_such_file_or_directory);

        const auto cwd = process->cwd();
        REQUIRE(!cwd);
        REQUIRE(cwd.error() == std::errc::no_such_file_or_directory);

        const auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == pid);
        REQUIRE(stat->comm == "(test)");
        REQUIRE(stat->state == 'Z');
        REQUIRE(stat->ppid == getpid());
        REQUIRE(stat->pgrp == getpgrp());
        REQUIRE(stat->session == getsid(pid));
        REQUIRE(*stat->exitCode == SIGKILL);

        const auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->name == "(test)");
        REQUIRE(status->state == "Z (zombie)");
        REQUIRE(status->tgid == pid);
        REQUIRE(status->pid == pid);
        REQUIRE(status->ppid == getpid());

        const auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE(tasks->size() == 1);
        REQUIRE(tasks->front() == pid);

        waitpid(pid, nullptr, 0);
    }

    SECTION("no such process") {
        const auto process = zero::os::procfs::process::open(99999);
        REQUIRE(!process);
        REQUIRE(process.error() == std::errc::no_such_file_or_directory);
    }

    prctl(PR_SET_NAME, name);
}
