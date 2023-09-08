#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <windows.h>
#include <filesystem>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::os::nt::process {
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

    tl::expected<Process, std::error_code> openProcess(DWORD pid);
}

#endif //ZERO_PROCESS_H
