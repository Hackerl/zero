#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#ifdef __linux__
#include <string>
#include <list>
#include <sys/types.h>
#include <optional>

namespace zero::proc {
    constexpr auto READ_PERMISSION = 0x1;
    constexpr auto WRITE_PERMISSION = 0x2;
    constexpr auto EXECUTE_PERMISSION = 0x4;
    constexpr auto SHARED_PERMISSION = 0x8;
    constexpr auto PRIVATE_PERMISSION = 0x10;

    struct ProcessMapping {
        uintptr_t start;
        uintptr_t end;
        int permissions;
        off_t offset;
        std::string device;
        ino_t inode;
        std::string pathname;
    };

    std::optional<ProcessMapping> getImageBase(pid_t pid, const std::string &path);
    std::optional<ProcessMapping> getAddressMapping(pid_t pid, uintptr_t address);
    std::optional<std::list<ProcessMapping>> getProcessMappings(pid_t pid);
    std::optional<std::list<pid_t>> getThreads(pid_t pid);
}
#endif

#endif //ZERO_PROCESS_H
