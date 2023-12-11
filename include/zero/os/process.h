#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#ifdef _WIN32
#include "nt/process.h"
#elif __APPLE__
#include "darwin/process.h"
#elif __linux__
#include "procfs/process.h"
#endif

namespace zero::os::process {
    using ID = int;

    struct CPUTime {
        double user;
        double system;
    };

    struct MemoryStat {
        std::uint64_t rss;
        std::uint64_t vms;
    };

    struct IOStat {
        std::uint64_t readBytes;
        std::uint64_t writeBytes;
    };

#ifdef _WIN32
    using ProcessImpl = nt::process::Process;
#elif __APPLE__
    using ProcessImpl = darwin::process::Process;
#elif __linux__
    using ProcessImpl = procfs::process::Process;
#endif

    class Process {
    public:
        explicit Process(ProcessImpl impl);

        [[nodiscard]] const ProcessImpl &impl();

        [[nodiscard]] ID pid() const;
        [[nodiscard]] tl::expected<ID, std::error_code> ppid() const;

        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> env() const;

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

    private:
        ProcessImpl mImpl;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(ID pid);
    tl::expected<std::list<ID>, std::error_code> all();
}

#endif //ZERO_PROCESS_H
