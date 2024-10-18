#ifndef ZERO_LINUX_PROCESS_H
#define ZERO_LINUX_PROCESS_H

#include "procfs/process.h"

#undef linux

namespace zero::os::linux::process {
    struct CPUTime {
        double user;
        double system;
    };

    struct MemoryStat {
        std::uint64_t rss;
        std::uint64_t vms;
    };

    using IOStat = procfs::process::IOStat;

    class Process {
    public:
        explicit Process(procfs::process::Process process);
        Process(Process &&rhs) noexcept;
        Process &operator=(Process &&rhs) noexcept;

        [[nodiscard]] pid_t pid() const;
        [[nodiscard]] tl::expected<pid_t, std::error_code> ppid() const;

        [[nodiscard]] tl::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> envs() const;
        [[nodiscard]] tl::expected<std::chrono::system_clock::time_point, std::error_code> startTime() const;

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

        tl::expected<void, std::error_code> kill(int sig);

    private:
        procfs::process::Process mProcess;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(pid_t pid);
    tl::expected<std::list<pid_t>, std::error_code> all();
}

#endif //ZERO_LINUX_PROCESS_H
