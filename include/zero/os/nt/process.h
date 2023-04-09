#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <windows.h>
#include <optional>
#include <filesystem>

namespace zero::os::nt::process {
    class Process {
    public:
        Process(HANDLE handle, DWORD pid);
        Process(Process &&rhs) noexcept;
        ~Process();

    public:
        [[nodiscard]] DWORD pid() const;
        [[nodiscard]] std::optional<DWORD> ppid() const;

    public:
        [[nodiscard]] std::optional<std::string> name() const;
        [[nodiscard]] std::optional<std::filesystem::path> image() const;
        [[nodiscard]] std::optional<std::vector<std::string>> cmdline() const;

    private:
        DWORD mPID;
        HANDLE mHandle;
    };

    std::optional<Process> openProcess(DWORD pid);
}

#endif //ZERO_PROCESS_H
