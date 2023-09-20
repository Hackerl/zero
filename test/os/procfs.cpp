#include <zero/os/procfs.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

using namespace std::chrono_literals;

TEST_CASE("linux procfs", "[procfs]") {
    char name[16] = {};
    pthread_getname_np(pthread_self(), name, sizeof(name));
    pthread_setname_np(pthread_self(), "(test)");

    SECTION("self") {
        auto pid = getpid();
        auto process = zero::os::procfs::openProcess(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        auto env = process->environ();
        REQUIRE(env);

        auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::find_if(
                mappings->begin(),
                mappings->end(),
                [](const zero::os::procfs::MemoryMapping &mapping) {
                    auto address = (uintptr_t) zero::filesystem::getApplicationPath;
                    return address >= mapping.start && address < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        auto permissions = zero::os::procfs::MemoryPermission::READ |
                           zero::os::procfs::MemoryPermission::EXECUTE |
                           zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        it = std::find_if(
                mappings->begin(),
                mappings->end(),
                [](const zero::os::procfs::MemoryMapping &mapping) {
                    auto address = (uintptr_t) &errno;
                    return address >= mapping.start && address < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        permissions = zero::os::procfs::MemoryPermission::READ |
                      zero::os::procfs::MemoryPermission::WRITE |
                      zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        auto mapping = process->findMapping((uintptr_t) &errno);
        REQUIRE(mapping);
        REQUIRE(mapping->permissions == permissions);

        mapping = process->getImageBase(path->string());
        REQUIRE(mapping);
        REQUIRE(mapping->permissions & zero::os::procfs::MemoryPermission::READ);

        auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == pid);
        REQUIRE(stat->comm == "(test)");
        REQUIRE(stat->state == 'R');
        REQUIRE(stat->ppid == getppid());
        REQUIRE(stat->pgrp == getpgrp());
        REQUIRE(stat->session == getsid(pid));

        auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->name == "(test)");
        REQUIRE(status->state == "R (running)");
        REQUIRE(status->tgid == pid);
        REQUIRE(status->pid == pid);
        REQUIRE(status->ppid == getppid());

        auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE(tasks->size() == 1);
        REQUIRE(tasks->front() == pid);
    }

    SECTION("child") {
        pid_t pid = fork();

        if (pid == 0) {
            pause();
            exit(0);
        }

        REQUIRE(pid > 0);
        std::this_thread::sleep_for(100ms);

        auto process = zero::os::procfs::openProcess(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename()) != std::string::npos);

        auto env = process->environ();
        REQUIRE(env);

        auto mappings = process->maps();
        REQUIRE(mappings);

        auto it = std::find_if(
                mappings->begin(),
                mappings->end(),
                [](const zero::os::procfs::MemoryMapping &mapping) {
                    auto address = (uintptr_t) zero::filesystem::getApplicationPath;
                    return address >= mapping.start && address < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        auto permissions = zero::os::procfs::MemoryPermission::READ |
                           zero::os::procfs::MemoryPermission::EXECUTE |
                           zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        it = std::find_if(
                mappings->begin(),
                mappings->end(),
                [](const zero::os::procfs::MemoryMapping &mapping) {
                    auto address = (uintptr_t) &errno;
                    return address >= mapping.start && address < mapping.end;
                }
        );
        REQUIRE(it != mappings->end());

        permissions = zero::os::procfs::MemoryPermission::READ |
                      zero::os::procfs::MemoryPermission::WRITE |
                      zero::os::procfs::MemoryPermission::PRIVATE;

        REQUIRE(it->permissions == permissions);

        auto mapping = process->findMapping((uintptr_t) &errno);
        REQUIRE(mapping);
        REQUIRE(mapping->permissions == permissions);

        mapping = process->getImageBase(path->string());
        REQUIRE(mapping);
        REQUIRE(mapping->permissions & zero::os::procfs::MemoryPermission::READ);

        auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == pid);
        REQUIRE(stat->comm == "(test)");
        REQUIRE(stat->state == 'S');
        REQUIRE(stat->ppid == getpid());
        REQUIRE(stat->pgrp == getpgrp());
        REQUIRE(stat->session == getsid(pid));

        auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->name == "(test)");
        REQUIRE(status->state == "S (sleeping)");
        REQUIRE(status->tgid == pid);
        REQUIRE(status->pid == pid);
        REQUIRE(status->ppid == getpid());

        auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE(tasks->size() == 1);
        REQUIRE(tasks->front() == pid);

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

        auto process = zero::os::procfs::openProcess(pid);
        REQUIRE(process);
        REQUIRE(process->pid() == pid);

        auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        auto comm = process->comm();
        REQUIRE(comm);
        REQUIRE(*comm == "(test)");

        auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->empty());

        auto env = process->environ();
        REQUIRE((env || env.error() == std::errc::permission_denied));

        auto mappings = process->maps();
        REQUIRE(mappings);
        REQUIRE(mappings->empty());

        auto mapping = process->findMapping((uintptr_t) &errno);
        REQUIRE(!mapping);
        REQUIRE(!mapping.error());

        mapping = process->getImageBase(path->string());
        REQUIRE(!mapping);
        REQUIRE(!mapping.error());

        auto exe = process->exe();
        REQUIRE(!exe);
        REQUIRE(exe.error() == std::errc::no_such_file_or_directory);

        auto cwd = process->cwd();
        REQUIRE(!cwd);
        REQUIRE(cwd.error() == std::errc::no_such_file_or_directory);

        auto stat = process->stat();
        REQUIRE(stat);
        REQUIRE(stat->pid == pid);
        REQUIRE(stat->comm == "(test)");
        REQUIRE(stat->state == 'Z');
        REQUIRE(stat->ppid == getpid());
        REQUIRE(stat->pgrp == getpgrp());
        REQUIRE(stat->session == getsid(pid));
        REQUIRE(*stat->exitCode == SIGKILL);

        auto status = process->status();
        REQUIRE(status);
        REQUIRE(status->name == "(test)");
        REQUIRE(status->state == "Z (zombie)");
        REQUIRE(status->tgid == pid);
        REQUIRE(status->pid == pid);
        REQUIRE(status->ppid == getpid());

        auto tasks = process->tasks();
        REQUIRE(tasks);
        REQUIRE(tasks->size() == 1);
        REQUIRE(tasks->front() == pid);

        waitpid(pid, nullptr, 0);
    }

    SECTION("no such process") {
        auto process = zero::os::procfs::openProcess(99999);
        REQUIRE(!process);
        REQUIRE(process.error() == std::errc::no_such_file_or_directory);
    }

    pthread_setname_np(pthread_self(), name);
}