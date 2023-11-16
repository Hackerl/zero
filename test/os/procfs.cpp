#include <zero/os/procfs.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <ranges>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

using namespace std::chrono_literals;

TEST_CASE("linux procfs", "[procfs]") {
    char name[16] = {};
    prctl(PR_GET_NAME, name);
    prctl(PR_SET_NAME, "(test)");

    const auto variable = reinterpret_cast<std::uintptr_t>(stdout);
    const auto function = reinterpret_cast<std::uintptr_t>(zero::filesystem::getApplicationPath);

    SECTION("all") {
        const auto ids = zero::os::procfs::all();
        REQUIRE(ids);
    }

    SECTION("self") {
        const auto pid = getpid();
        const auto process = zero::os::procfs::self();
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

        const auto env = process->env();
        REQUIRE(env);

        const auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::ranges::find_if(
                *mappings,
                [=](const zero::os::procfs::MemoryMapping &mapping) {
                    return function >= mapping.start && function < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        auto permissions = zero::os::procfs::MemoryPermission::READ |
                           zero::os::procfs::MemoryPermission::EXECUTE |
                           zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        it = std::ranges::find_if(
                *mappings,
                [=](const zero::os::procfs::MemoryMapping &mapping) {
                    return variable >= mapping.start && variable < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        permissions = zero::os::procfs::MemoryPermission::READ |
                      zero::os::procfs::MemoryPermission::WRITE |
                      zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        auto mapping = process->findMapping(variable);
        REQUIRE(mapping);
        REQUIRE(mapping->permissions == permissions);

        mapping = process->getImageBase(path->string());
        REQUIRE(mapping);
        REQUIRE(mapping->permissions & zero::os::procfs::MemoryPermission::READ);

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

        const auto process = zero::os::procfs::open(pid);
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

        const auto env = process->env();
        REQUIRE(env);

        const auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::ranges::find_if(
                *mappings,
                [=](const zero::os::procfs::MemoryMapping &mapping) {
                    return function >= mapping.start && function < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        auto permissions = zero::os::procfs::MemoryPermission::READ |
                           zero::os::procfs::MemoryPermission::EXECUTE |
                           zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        it = std::ranges::find_if(
                *mappings,
                [=](const zero::os::procfs::MemoryMapping &mapping) {
                    return variable >= mapping.start && variable < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        permissions = zero::os::procfs::MemoryPermission::READ |
                      zero::os::procfs::MemoryPermission::WRITE |
                      zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        auto mapping = process->findMapping(variable);
        REQUIRE(mapping);
        REQUIRE(mapping->permissions == permissions);

        mapping = process->getImageBase(path->string());
        REQUIRE(mapping);
        REQUIRE(mapping->permissions & zero::os::procfs::MemoryPermission::READ);

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

        const auto process = zero::os::procfs::open(pid);
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

        const auto env = process->env();
        REQUIRE(!env);

        const auto mappings = process->maps();
        REQUIRE(!mappings);
        REQUIRE(mappings.error() == zero::os::procfs::Error::MAYBE_ZOMBIE_PROCESS);

        auto mapping = process->findMapping(function);
        REQUIRE(!mapping);
        REQUIRE(mapping.error() == zero::os::procfs::Error::MAYBE_ZOMBIE_PROCESS);

        mapping = process->getImageBase(path->string());
        REQUIRE(!mapping);
        REQUIRE(mapping.error() == zero::os::procfs::Error::MAYBE_ZOMBIE_PROCESS);

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
        const auto process = zero::os::procfs::open(99999);
        REQUIRE(!process);
        REQUIRE(process.error() == std::errc::no_such_file_or_directory);
    }

    prctl(PR_SET_NAME, name);
}