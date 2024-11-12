#ifndef ZERO_LINUX_PROCESS_H
#define ZERO_LINUX_PROCESS_H

#include "procfs/process.h"

#undef linux

namespace zero::os::linux::process {
    struct CPUTime {
        double user{};
        double system{};
    };

    struct MemoryStat {
        std::uint64_t rss{};
        std::uint64_t vms{};
    };

    using IOStat = procfs::process::IOStat;

    class Process {
    public:
        explicit Process(procfs::process::Process process);
        Process(Process &&rhs) noexcept;
        Process &operator=(Process &&rhs) noexcept;

        [[nodiscard]] pid_t pid() const;
        [[nodiscard]] std::expected<pid_t, std::error_code> ppid() const;

        [[nodiscard]] std::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] std::expected<std::map<std::string, std::string>, std::error_code> envs() const;
        [[nodiscard]] std::expected<std::chrono::system_clock::time_point, std::error_code> startTime() const;

        [[nodiscard]] std::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] std::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] std::expected<IOStat, std::error_code> io() const;

        std::expected<void, std::error_code> kill(int sig);

    private:
        procfs::process::Process mProcess;
    };

    std::expected<Process, std::error_code> self();
    std::expected<Process, std::error_code> open(pid_t pid);
    std::expected<std::list<pid_t>, std::error_code> all();
}

#endif //ZERO_LINUX_PROCESS_H
