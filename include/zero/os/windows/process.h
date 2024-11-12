#ifndef ZERO_WINDOWS_PROCESS_H
#define ZERO_WINDOWS_PROCESS_H

#include <map>
#include <list>
#include <vector>
#include <chrono>
#include <optional>
#include <windows.h>
#include <filesystem>
#include <zero/error.h>
#include <tl/expected.hpp>

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
        DEFINE_ERROR_CODE_INNER_EX(
            Error,
            "zero::os::windows::process::Process",
            API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported,
            PROCESS_STILL_ACTIVE, "process still active", std::errc::operation_would_block,
            UNEXPECTED_DATA, "unexpected data", DEFAULT_ERROR_CONDITION,
            WAIT_PROCESS_TIMEOUT, "wait process timeout", std::errc::timed_out
        )

        Process(HANDLE handle, DWORD pid);
        Process(Process &&rhs) noexcept;
        Process &operator=(Process &&rhs) noexcept;
        ~Process();

        static tl::expected<Process, std::error_code> from(HANDLE handle);

    private:
        [[nodiscard]] tl::expected<std::uintptr_t, std::error_code> parameters() const;

    public:
        [[nodiscard]] HANDLE handle() const;

        [[nodiscard]] DWORD pid() const;
        [[nodiscard]] tl::expected<DWORD, std::error_code> ppid() const;

        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> envs() const;
        [[nodiscard]] tl::expected<std::chrono::system_clock::time_point, std::error_code> startTime() const;

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

        [[nodiscard]] tl::expected<DWORD, std::error_code> exitCode() const;
        [[nodiscard]] tl::expected<void, std::error_code> wait(std::optional<std::chrono::milliseconds> timeout) const;

        tl::expected<void, std::error_code> terminate(DWORD code);

    private:
        DWORD mPID;
        HANDLE mHandle;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(DWORD pid);
    tl::expected<std::list<DWORD>, std::error_code> all();
}

DECLARE_ERROR_CODE(zero::os::windows::process::Process::Error)

#endif //ZERO_WINDOWS_PROCESS_H
