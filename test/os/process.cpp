#include <zero/os/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include <sys/wait.h>

TEST_CASE("linux procfs", "[process]") {
    pthread_setname_np(pthread_self(), "(test)");

    pid_t pid = fork();

    if (!pid) {
        pause();
        exit(0);
    }

    zero::os::process::Process process(pid);

    REQUIRE(process.cmdline());
    REQUIRE(process.environ());
    REQUIRE(process.maps());
    REQUIRE(*process.comm() == "(test)");
    REQUIRE(*process.tasks() == std::list<pid_t>{pid});
    REQUIRE(*process.exe() == zero::filesystem::getApplicationPath());
    REQUIRE(*process.cwd() == zero::filesystem::getApplicationDirectory());

    std::optional<zero::os::process::Stat> stat = process.stat();

    REQUIRE(stat);
    REQUIRE(stat->pid == pid);
    REQUIRE(stat->ppid == getpid());
    REQUIRE(stat->comm == "(test)");

    std::optional<zero::os::process::Status> status = process.status();

    REQUIRE(status);
    REQUIRE(status->pid == pid);
    REQUIRE(status->ppid == getpid());
    REQUIRE(status->name == "(test)");

    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}