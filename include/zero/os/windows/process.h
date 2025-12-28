#ifndef ZERO_OS_WINDOWS_PROCESS_H
#define ZERO_OS_WINDOWS_PROCESS_H

#include <map>
#include <list>
#include <chrono>
#include <optional>
#include <filesystem>
#include <zero/os/resource.h>

namespace zero::os::windows::process {
    struct CPUTime {
        double user{};
        double system{};
    };

    struct MemoryStat {
        std::uint64_t rss{};
        std::uint64_t vms{};
    };

    struct IOStat {
        std::uint64_t readCount{};
        std::uint64_t readBytes{};
        std::uint64_t writeCount{};
        std::uint64_t writeBytes{};
    };

    class Process {
    public:
        Z_DEFINE_ERROR_CODE_INNER_EX(
            Error,
            "zero::os::windows::process::Process",
            API_NOT_AVAILABLE, "API not available", std::errc::function_not_supported,
            PROCESS_STILL_ACTIVE, "Process still active", std::errc::operation_would_block,
            UNEXPECTED_DATA, "Unexpected data", Z_DEFAULT_ERROR_CONDITION,
            WAIT_PROCESS_TIMEOUT, "Process wait timed out", std::errc::timed_out
        )

        Process(Resource resource, DWORD pid);

        static std::expected<Process, std::error_code> from(HANDLE handle);

    private:
        [[nodiscard]] std::expected<std::uintptr_t, std::error_code> parameters() const;

    public:
        [[nodiscard]] const Resource &handle() const;

        [[nodiscard]] DWORD pid() const;
        [[nodiscard]] std::expected<DWORD, std::error_code> ppid() const;

        [[nodiscard]] std::expected<std::string, std::error_code> name() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] std::expected<std::map<std::string, std::string>, std::error_code> envs() const;
        [[nodiscard]] std::expected<std::chrono::system_clock::time_point, std::error_code> startTime() const;

        [[nodiscard]] std::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] std::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] std::expected<IOStat, std::error_code> io() const;

        [[nodiscard]] std::expected<DWORD, std::error_code> exitCode() const;
        [[nodiscard]] std::expected<void, std::error_code> wait(std::optional<std::chrono::milliseconds> timeout) const;

        [[nodiscard]] std::expected<std::string, std::error_code> user() const;

        std::expected<void, std::error_code> terminate(DWORD code);

    private:
        Resource mResource;
        DWORD mPID;
    };

    std::expected<Process, std::error_code> self();
    std::expected<Process, std::error_code> open(DWORD pid);
    std::expected<std::list<DWORD>, std::error_code> all();
}

Z_DECLARE_ERROR_CODE(zero::os::windows::process::Process::Error)

#endif //ZERO_OS_WINDOWS_PROCESS_H
