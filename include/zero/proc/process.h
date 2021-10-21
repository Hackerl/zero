#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <string>
#include <list>

namespace zero {
    namespace proc {
        struct CProcessMapping {
            unsigned long start;
            unsigned long end;
            std::string permissions;
            unsigned long offset;
            std::string device;
            unsigned long inode;
            std::string pathname;
        };

        bool getImageBase(pid_t pid, const std::string &path, CProcessMapping &processMapping);
        bool getProcessMappings(pid_t pid, std::list<CProcessMapping> &processMappings);

        bool getThreads(pid_t pid, std::list<pid_t> &threads);
    }
}

#endif //ZERO_PROCESS_H
