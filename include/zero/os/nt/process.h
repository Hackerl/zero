#ifndef ZERO_NT_PROCESS_H
#define ZERO_NT_PROCESS_H

#include <map>
#include <windows.h>
#include <filesystem>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::os::nt::process {
    enum Error {
        API_NOT_AVAILABLE = 1,
        PROCESS_STILL_ACTIVE,
        UNEXPECTED_DATA
    };

    class ErrorCategory final : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
        [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
    };

    const std::error_category &errorCategory();
    std::error_code make_error_code(Error e);

    struct CPUTime {
        double user;
        double system;
    };

    struct MemoryStat {
        std::uint64_t rss;
        std::uint64_t vms;
    };

    struct IOStat {
        std::uint64_t readCount;
        std::uint64_t readBytes;
        std::uint64_t writeCount;
        std::uint64_t writeBytes;
    };

    class Process {
    public:
        Process(HANDLE handle, DWORD pid);
        Process(Process &&rhs) noexcept;
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

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

        [[nodiscard]] tl::expected<DWORD, std::error_code> exitCode() const;

        [[nodiscard]] tl::expected<void, std::error_code>
        wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt) const;

        [[nodiscard]] tl::expected<void, std::error_code> tryWait() const;

        tl::expected<void, std::error_code> terminate(DWORD code);

    private:
        DWORD mPID;
        HANDLE mHandle;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(DWORD pid);
    tl::expected<std::list<DWORD>, std::error_code> all();
}

template<>
struct std::is_error_code_enum<zero::os::nt::process::Error> : std::true_type {
};

#endif //ZERO_NT_PROCESS_H
