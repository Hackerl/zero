#ifndef ZERO_NT_PROCESS_H
#define ZERO_NT_PROCESS_H

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

    class Process {
    public:
        Process(HANDLE handle, DWORD pid);
        Process(Process &&rhs) noexcept;
        ~Process();

    public:
        [[nodiscard]] DWORD pid() const;
        [[nodiscard]] tl::expected<DWORD, std::error_code> ppid() const;

    public:
        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> image() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;

    private:
        DWORD mPID;
        HANDLE mHandle;
    };

    tl::expected<Process, std::error_code> open(DWORD pid);
}

namespace std {
    template<>
    struct is_error_code_enum<zero::os::nt::process::Error> : public true_type {

    };
}

#endif //ZERO_NT_PROCESS_H
