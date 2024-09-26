#include <zero/os/procfs/process.h>
#include <zero/filesystem/fs.h>
#include <zero/os/unix/error.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

TEST_CASE("procfs process", "[procfs]") {
    std::array<char, 16> name{};

    REQUIRE(zero::os::unix::expected([&] {
        return prctl(PR_GET_NAME, name.data());
    }));

    REQUIRE(zero::os::unix::expected([] {
        return prctl(PR_SET_NAME, "(test)");
    }));

    const auto variable = reinterpret_cast<std::uintptr_t>(stdout);
    const auto function = reinterpret_cast<std::uintptr_t>(zero::filesystem::applicationPath);

    SECTION("all") {
        const auto ids = zero::os::procfs::process::all();
        REQUIRE(ids);
    }

    SECTION("self") {
        const auto pid = getpid();
        const auto process = zero::os::procfs::process::self();
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

        const auto env = process->environ();
        REQUIRE(env);

        const auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::ranges::find_if(
            *mappings,
            [=](const auto &mapping) {
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
        REQUIRE_THAT(*tasks, !Catch::Matchers::IsEmpty());
        REQUIRE_THAT(*tasks, Catch::Matchers::Contains(pid));

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

        const auto process = zero::os::procfs::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

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
        REQUIRE_THAT(*tasks, Catch::Matchers::SizeIs(1));
        REQUIRE_THAT(*tasks, Catch::Matchers::Contains(pid));

        const auto io = process->io();
        REQUIRE(io);

        REQUIRE(zero::os::unix::expected([=] {
            return kill(pid, SIGKILL);
        }));

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
        REQUIRE(zero::os::unix::expected([=] {
            return kill(pid, SIGKILL);
        }));

        std::this_thread::sleep_for(100ms);

        const auto process = zero::os::procfs::process::open(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        const auto cmdline = process->cmdline();
        REQUIRE_FALSE(cmdline);
        REQUIRE(cmdline.error() == zero::os::procfs::process::Process::Error::MAYBE_ZOMBIE_PROCESS);

        const auto env = process->environ();
        REQUIRE((!env || env->empty()));

        const auto mappings = process->maps();
        REQUIRE_FALSE(mappings);
        REQUIRE(mappings.error() == zero::os::procfs::process::Process::Error::MAYBE_ZOMBIE_PROCESS);

        const auto exe = process->exe();
        REQUIRE_FALSE(exe);
        REQUIRE(exe.error() == std::errc::no_such_file_or_directory);

        const auto cwd = process->cwd();
        REQUIRE_FALSE(cwd);
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
        REQUIRE_THAT(*tasks, Catch::Matchers::SizeIs(1));
        REQUIRE_THAT(*tasks, Catch::Matchers::Contains(pid));

        const auto id = zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        });
        REQUIRE(id);
        REQUIRE(*id == pid);
    }

    SECTION("no such process") {
        const auto process = zero::os::procfs::process::open(99999);
        REQUIRE_FALSE(process);
        REQUIRE(process.error() == std::errc::no_such_file_or_directory);
    }

    REQUIRE(zero::os::unix::expected([&] {
        return prctl(PR_SET_NAME, name.data());
    }));
}
