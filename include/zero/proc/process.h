#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <string>
#include <list>
#include <sys/types.h>

namespace zero {
    namespace proc {
#ifdef __linux__
        constexpr auto READ_PERMISSION = 0x1;
        constexpr auto WRITE_PERMISSION = 0x2;
        constexpr auto EXECUTE_PERMISSION = 0x4;
        constexpr auto SHARED_PERMISSION = 0x8;
        constexpr auto PRIVATE_PERMISSION = 0x10;

        struct CProcessMapping {
            uintptr_t start;
            uintptr_t end;
            int permissions;
            off_t offset;
            std::string device;
            ino_t inode;
            std::string pathname;
        };

        bool getImageBase(pid_t pid, const std::string &path, CProcessMapping &processMapping);
        bool getAddressMapping(pid_t pid, uintptr_t address, CProcessMapping &processMapping);
        bool getProcessMappings(pid_t pid, std::list<CProcessMapping> &processMappings);

        bool getThreads(pid_t pid, std::list<pid_t> &threads);
#endif
    }
}

#endif //ZERO_PROCESS_H
