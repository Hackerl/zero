#ifndef ZERO_OS_MACOS_PROCESS_H
#define ZERO_OS_MACOS_PROCESS_H

#include <map>
#include <list>
#include <vector>
#include <filesystem>
#include <zero/error.h>

namespace zero::os::macos::process {
    struct CPUTime {
        double user;
        double system;
    };

    struct MemoryStat {
        std::uint64_t rss;
        std::uint64_t vms;
        std::uint64_t swap;
    };

    struct IOStat {
        std::uint64_t readBytes;
        std::uint64_t writeBytes;
    };

    class Process {
    public:
        Z_DEFINE_ERROR_CODE_INNER(
            Error,
            "zero::os::macos::process::Process",
            NoSuchUser, "No such user"
        )

        explicit Process(pid_t pid);
        Process(Process &&rhs) noexcept;
        Process &operator=(Process &&rhs) noexcept;

    private:
        [[nodiscard]] std::expected<std::vector<char>, std::error_code> arguments() const;

    public:
        [[nodiscard]] pid_t pid() const;
        [[nodiscard]] std::expected<pid_t, std::error_code> ppid() const;

        [[nodiscard]] std::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] std::expected<std::string, std::error_code> name() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] std::expected<std::map<std::string, std::string>, std::error_code> envs() const;
        [[nodiscard]] std::expected<std::chrono::system_clock::time_point, std::error_code> startTime() const;

        [[nodiscard]] std::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] std::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] std::expected<IOStat, std::error_code> io() const;

        [[nodiscard]] std::expected<std::string, std::error_code> user() const;

        std::expected<void, std::error_code> kill(int sig);

    private:
        pid_t mPID;
    };

    Process self();
    std::expected<Process, std::error_code> open(pid_t pid);
    std::list<pid_t> all();
}

Z_DECLARE_ERROR_CODE(zero::os::macos::process::Process::Error)

#endif //ZERO_OS_MACOS_PROCESS_H
