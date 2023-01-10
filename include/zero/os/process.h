#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#ifdef __linux__
#include <string>
#include <list>
#include <sys/types.h>
#include <optional>

namespace zero::os::process {
    enum MemoryPermission {
        READ = 0x1,
        WRITE = 0x2,
        EXECUTE = 0x4,
        SHARED = 0x8,
        PRIVATE = 0x10
    };

    struct MemoryMapping {
        uintptr_t start;
        uintptr_t end;
        int permissions;
        off_t offset;
        std::string device;
        ino_t inode;
        std::string pathname;
    };

    struct Stat {
        pid_t pid;
        std::string comm;
        char state;
        pid_t ppid;
        pid_t pgrp;
        int session;
        int tty;
        pid_t tpgid;
        unsigned int flags;
        unsigned long minFlt;
        unsigned long cMinFlt;
        unsigned long majFlt;
        unsigned long cMajFlt;
        unsigned long uTime;
        unsigned long sTime;
        unsigned long cuTime;
        unsigned long csTime;
        long priority;
        long nice;
        long numThreads;
        long intervalValue;
        unsigned long long startTime;
        unsigned long vSize;
        long rss;
        unsigned long rssLimit;
        unsigned long startCode;
        unsigned long endCode;
        unsigned long startStack;
        unsigned long esp;
        unsigned long eip;
        unsigned long signal;
        unsigned long blocked;
        unsigned long sigIgnore;
        unsigned long sigCatch;
        unsigned long wChan;
        unsigned long nSwap;
        unsigned long cnSwap;
        std::optional<int> exitSignal;
        std::optional<int> processor;
        std::optional<unsigned int> rtPriority;
        std::optional<unsigned int> policy;
        std::optional<unsigned long long> delayAcctBlkIOTicks;
        std::optional<unsigned long> guestTime;
        std::optional<long> cGuestTime;
        std::optional<unsigned long> startData;
        std::optional<unsigned long> endData;
        std::optional<unsigned long> startBrk;
        std::optional<unsigned long> argStart;
        std::optional<unsigned long> argEnd;
        std::optional<unsigned long> envStart;
        std::optional<unsigned long> envEnd;
        std::optional<int> exitCode;
    };

    std::optional<MemoryMapping> getImageBase(pid_t pid, std::string_view path);
    std::optional<MemoryMapping> getAddressMapping(pid_t pid, uintptr_t address);
    std::optional<std::list<MemoryMapping>> getProcessMappings(pid_t pid);
    std::optional<std::list<pid_t>> getThreads(pid_t pid);
    std::optional<Stat> getProcessStat(pid_t pid);
}
#endif

#endif //ZERO_PROCESS_H
