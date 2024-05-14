 #ifndef ZERO_DARWIN_PROCESS_H
#define ZERO_DARWIN_PROCESS_H

#include <map>
#include <list>
#include <vector>
#include <filesystem>
#include <tl/expected.hpp>
#include <zero/error.h>

namespace zero::os::darwin::process {
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
        DEFINE_ERROR_CODE_TYPES(
            Error,
            "zero::os::darwin::process::Process",
            UNEXPECTED_DATA, "unexpected data"
        )

        explicit Process(pid_t pid);
        Process(Process &&rhs) noexcept;
        Process &operator=(Process &&rhs) noexcept;

    private:
        [[nodiscard]] tl::expected<std::vector<char>, std::error_code> arguments() const;

    public:
        [[nodiscard]] pid_t pid() const;
        [[nodiscard]] tl::expected<pid_t, std::error_code> ppid() const;

        [[nodiscard]] tl::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> envs() const;

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

        tl::expected<void, std::error_code> kill(int sig);

    private:
        pid_t mPID;
    };

    DEFINE_MAKE_ERROR_CODE(Process::Error)

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(pid_t pid);
    tl::expected<std::list<pid_t>, std::error_code> all();
}

DECLARE_ERROR_CODE(zero::os::darwin::process::Process::Error)

#endif //ZERO_DARWIN_PROCESS_H
