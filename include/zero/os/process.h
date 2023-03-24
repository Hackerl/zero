#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <windows.h>
#include <optional>
#include <filesystem>

namespace zero::os::process {
    class Process {
    public:
        Process(HANDLE handle, int pid);
        Process(Process &&rhs) noexcept;
        ~Process();

    public:
        [[nodiscard]] int pid() const;

    public:
        [[nodiscard]] std::optional<std::string> name() const;
        [[nodiscard]] std::optional<std::filesystem::path> image() const;
        [[nodiscard]] std::optional<std::vector<std::string>> cmdline() const;

    private:
        int mPID;
        HANDLE mHandle;
    };

    std::optional<Process> openProcess(int pid);
}

#endif //ZERO_PROCESS_H
