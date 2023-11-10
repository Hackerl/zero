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
        UNEXPECTED_DATA
    };

    class ErrorCategory : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
        [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
    };

    const std::error_category &errorCategory();
    std::error_code make_error_code(Error e);

    struct CPUStat {
        double user;
        double system;
    };

    struct MemoryStat {
        uint64_t rss;
        uint64_t vms;
    };

    struct IOStat {
        uint64_t readCount;
        uint64_t readBytes;
        uint64_t writeCount;
        uint64_t writeBytes;
    };

    class Process {
    public:
        Process(HANDLE handle, DWORD pid);
        Process(Process &&rhs) noexcept;
        ~Process();

    public:
        [[nodiscard]] HANDLE handle() const;

    public:
        [[nodiscard]] DWORD pid() const;
        [[nodiscard]] tl::expected<DWORD, std::error_code> ppid() const;

    public:
        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> env() const;

    public:
        [[nodiscard]] tl::expected<CPUStat, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

    private:
        [[nodiscard]] tl::expected<uintptr_t, std::error_code> parameters() const;

    private:
        DWORD mPID;
        HANDLE mHandle;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(DWORD pid);
    tl::expected<std::list<DWORD>, std::error_code> all();
}

namespace std {
    template<>
    struct is_error_code_enum<zero::os::nt::process::Error> : public true_type {

    };
}

#endif //ZERO_NT_PROCESS_H
