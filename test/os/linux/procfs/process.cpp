#include <catch_extensions.h>
#include <zero/os/linux/procfs/process.h>
#include <zero/filesystem/fs.h>
#include <zero/os/unix/error.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

TEST_CASE("list process ids", "[os::linux::procfs::process]") {
    const auto ids = zero::os::linux::procfs::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(getpid()));
}

TEST_CASE("self process", "[os::linux::procfs::process]") {
    const auto variable = reinterpret_cast<std::uintptr_t>(stdout);
    const auto function = reinterpret_cast<std::uintptr_t>(zero::filesystem::applicationPath);

    const auto pid = getpid();
    const auto process = zero::os::linux::procfs::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

    REQUIRE(process->comm() == "zero_test");

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

    const auto envs = process->environ();
    REQUIRE(envs);

    const auto mappings = process->maps();
    REQUIRE(mappings);

    auto it = std::ranges::find_if(
        *mappings,
        [=](const auto &mapping) {
            return function >= mapping.start && function < mapping.end;
        }
    );
    REQUIRE(it != mappings->end());
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::READ));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::EXECUTE));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::PRIVATE));

    it = std::ranges::find_if(
        *mappings,
        [=](const zero::os::linux::procfs::process::MemoryMapping &mapping) {
            return variable >= mapping.start && variable < mapping.end;
        }
    );
    REQUIRE(it != mappings->end());
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::READ));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::WRITE));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::PRIVATE));

    REQUIRE(process->exe() == *path);
    REQUIRE(process->cwd() == zero::filesystem::currentPath());

    const auto stat = process->stat();
    REQUIRE(stat);
    REQUIRE(stat->pid == pid);
    REQUIRE(stat->comm == "zero_test");
    REQUIRE(stat->state == 'R');
    REQUIRE(stat->ppid == getppid());
    REQUIRE(stat->processGroupID == getpgrp());
    REQUIRE(stat->sessionID == getsid(pid));

    const auto status = process->status();
    REQUIRE(status);
    REQUIRE(status->name == "zero_test");
    REQUIRE(status->state == "R (running)");
    REQUIRE(status->threadGroupID == pid);
    REQUIRE(status->pid == pid);
    REQUIRE(status->ppid == getppid());

    const auto tasks = process->tasks();
    REQUIRE(tasks);
    REQUIRE_THAT(*tasks, !Catch::Matchers::IsEmpty());
    REQUIRE_THAT(*tasks, Catch::Matchers::Contains(pid));

    const auto io = process->io();
    REQUIRE(io);
}

TEST_CASE("child process", "[os::linux::procfs::process]") {
    using namespace std::chrono_literals;

    const auto variable = reinterpret_cast<std::uintptr_t>(stdout);
    const auto function = reinterpret_cast<std::uintptr_t>(zero::filesystem::applicationPath);

    const auto pid = fork();

    if (pid == 0) {
        pause();
        std::exit(EXIT_FAILURE);
    }

    REQUIRE(pid > 0);
    std::this_thread::sleep_for(100ms);

    const auto process = zero::os::linux::procfs::process::open(pid);
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

    REQUIRE(process->comm() == "zero_test");

    const auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

    const auto envs = process->environ();
    REQUIRE(envs);

    const auto mappings = process->maps();
    REQUIRE(mappings);

    auto it = std::ranges::find_if(
        *mappings,
        [=](const zero::os::linux::procfs::process::MemoryMapping &mapping) {
            return function >= mapping.start && function < mapping.end;
        }
    );
    REQUIRE(it != mappings->end());
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::READ));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::EXECUTE));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::PRIVATE));

    it = std::ranges::find_if(
        *mappings,
        [=](const zero::os::linux::procfs::process::MemoryMapping &mapping) {
            return variable >= mapping.start && variable < mapping.end;
        }
    );
    REQUIRE(it != mappings->end());
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::READ));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::WRITE));
    REQUIRE(it->permissions.test(zero::os::linux::procfs::process::MemoryPermission::PRIVATE));

    REQUIRE(process->exe() == *path);
    REQUIRE(process->cwd() == zero::filesystem::currentPath());

    const auto stat = process->stat();
    REQUIRE(stat);
    REQUIRE(stat->pid == pid);
    REQUIRE(stat->comm == "zero_test");
    REQUIRE(stat->state == 'S');
    REQUIRE(stat->ppid == getpid());
    REQUIRE(stat->processGroupID == getpgrp());
    REQUIRE(stat->sessionID == getsid(pid));

    const auto status = process->status();
    REQUIRE(status);
    REQUIRE(status->name == "zero_test");
    REQUIRE(status->state == "S (sleeping)");
    REQUIRE(status->threadGroupID == pid);
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

    REQUIRE(
        zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }) == pid
    );
}

TEST_CASE("zombie process", "[os::linux::procfs::process]") {
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

    const auto process = zero::os::linux::procfs::process::open(pid);
    REQUIRE(process);
    REQUIRE(process->pid() == pid);

    REQUIRE(process->comm() == "zero_test");

    const auto cmdline = process->cmdline();
    REQUIRE_FALSE(cmdline);
    REQUIRE(cmdline.error() == zero::os::linux::procfs::process::Process::Error::MAYBE_ZOMBIE_PROCESS);

    const auto envs = process->environ();
    REQUIRE((!envs || envs->empty()));

    const auto mappings = process->maps();
    REQUIRE_FALSE(mappings);
    REQUIRE(mappings.error() == zero::os::linux::procfs::process::Process::Error::MAYBE_ZOMBIE_PROCESS);

    const auto exe = process->exe();
    REQUIRE_FALSE(exe);
    REQUIRE(exe.error() == std::errc::no_such_file_or_directory);

    const auto cwd = process->cwd();
    REQUIRE_FALSE(cwd);
    REQUIRE(cwd.error() == std::errc::no_such_file_or_directory);

    const auto stat = process->stat();
    REQUIRE(stat);
    REQUIRE(stat->pid == pid);
    REQUIRE(stat->comm == "zero_test");
    REQUIRE(stat->state == 'Z');
    REQUIRE(stat->ppid == getpid());
    REQUIRE(stat->processGroupID == getpgrp());
    REQUIRE(stat->sessionID == getsid(pid));
    REQUIRE(stat->exitCode == SIGKILL);

    const auto status = process->status();
    REQUIRE(status);
    REQUIRE(status->name == "zero_test");
    REQUIRE(status->state == "Z (zombie)");
    REQUIRE(status->threadGroupID == pid);
    REQUIRE(status->pid == pid);
    REQUIRE(status->ppid == getpid());

    const auto tasks = process->tasks();
    REQUIRE(tasks);
    REQUIRE_THAT(*tasks, Catch::Matchers::SizeIs(1));
    REQUIRE_THAT(*tasks, Catch::Matchers::Contains(pid));

    REQUIRE(
        zero::os::unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }) == pid
    );
}

TEST_CASE("open process failed", "[os::linux::procfs::process]") {
    const auto process = zero::os::linux::procfs::process::open(99999);
    REQUIRE_FALSE(process);
    REQUIRE(process.error() == std::errc::no_such_file_or_directory);
}
