#include <zero/os/procfs.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include <sys/wait.h>

TEST_CASE("linux procfs", "[procfs]") {
    pthread_setname_np(pthread_self(), "(test)");

    pid_t pid = fork();

    if (!pid) {
        pause();
        exit(0);
    }

    std::optional<zero::os::procfs::Process> process = zero::os::procfs::openProcess(pid);

    REQUIRE(process);

    REQUIRE(process->cmdline());
    REQUIRE(process->environ());
    REQUIRE(process->maps());
    REQUIRE(*process->comm() == "(test)");
    REQUIRE(*process->tasks() == std::list<pid_t>{pid});
    REQUIRE(*process->exe() == zero::filesystem::getApplicationPath());
    REQUIRE(*process->cwd() == zero::filesystem::getApplicationDirectory());

    std::optional<zero::os::procfs::Stat> stat = process->stat();

    REQUIRE(stat);
    REQUIRE(stat->pid == pid);
    REQUIRE(stat->ppid == getpid());
    REQUIRE(stat->comm == "(test)");

    std::optional<zero::os::procfs::Status> status = process->status();

    REQUIRE(status);
    REQUIRE(status->pid == pid);
    REQUIRE(status->ppid == getpid());
    REQUIRE(status->name == "(test)");

    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}