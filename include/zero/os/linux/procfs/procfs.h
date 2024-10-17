#ifndef ZERO_PROCFS_H
#define ZERO_PROCFS_H

#include <vector>
#include <cstdint>
#include <optional>
#include <expected>
#include <zero/error.h>

#undef linux

namespace zero::os::linux::procfs {
    DEFINE_ERROR_CODE(
        Error,
        "zero::os::linux::procfs",
        UNEXPECTED_DATA, "unexpected data"
    )

    struct CPUTime {
        std::uint64_t user;
        std::uint64_t nice;
        std::uint64_t system;
        std::uint64_t idle;
        std::optional<std::uint64_t> ioWait;
        std::optional<std::uint64_t> irq;
        std::optional<std::uint64_t> softIRQ;
        std::optional<std::uint64_t> steal;
        std::optional<std::uint64_t> guest;
        std::optional<std::uint64_t> guestNice;
    };

    struct Stat {
        CPUTime total;
        std::vector<CPUTime> cpuTimes;
        std::uint64_t contextSwitches;
        std::uint64_t bootTime;
        std::uint64_t processesCreated;
        std::optional<std::uint32_t> processesRunning;
        std::optional<std::uint32_t> processesBlocked;
    };

    std::expected<Stat, std::error_code> stat();

    struct MemoryStat {
        std::uint64_t memoryTotal;
        std::uint64_t memoryFree;
        std::optional<std::uint64_t> memoryAvailable;
        std::uint64_t buffers;
        std::uint64_t cached;
        std::uint64_t swapCached;
        std::uint64_t active;
        std::uint64_t inactive;
        std::optional<std::uint64_t> activeAnonymous;
        std::optional<std::uint64_t> inactiveAnonymous;
        std::optional<std::uint64_t> activeFile;
        std::optional<std::uint64_t> inactiveFile;
        std::optional<std::uint64_t> unevictable;
        std::optional<std::uint64_t> mLocked;
        std::optional<std::uint64_t> highTotal;
        std::optional<std::uint64_t> highFree;
        std::optional<std::uint64_t> lowTotal;
        std::optional<std::uint64_t> lowFree;
        std::optional<std::uint64_t> mmapCopy;
        std::uint64_t swapTotal;
        std::uint64_t swapFree;
        std::uint64_t dirty;
        std::uint64_t writeBack;
        std::optional<std::uint64_t> anonymousPages;
        std::uint64_t mapped;
        std::optional<std::uint64_t> sharedMemory;
        std::uint64_t slab;
        std::optional<std::uint64_t> reclaimableSlab;
        std::optional<std::uint64_t> unreclaimableSlab;
        std::optional<std::uint64_t> kernelStack;
        std::optional<std::uint64_t> pageTables;
        std::optional<std::uint64_t> secondaryPageTables;
        std::optional<std::uint64_t> quickLists;
        std::optional<std::uint64_t> nfsUnstable;
        std::optional<std::uint64_t> bounceBuffers;
        std::optional<std::uint64_t> writeBackTemporary;
        std::optional<std::uint64_t> commitLimit;
        std::uint64_t committedAs;
        std::uint64_t vmallocTotal;
        std::uint64_t vmallocUsed;
        std::uint64_t vmallocChunk;
        std::optional<std::uint64_t> hardwareCorrupted;
        std::optional<std::uint64_t> anonymousHugePages;
        std::optional<std::uint64_t> sharedMemoryHugePages;
        std::optional<std::uint64_t> sharedMemoryPMDMapped;
        std::optional<std::uint64_t> cmaTotal;
        std::optional<std::uint64_t> cmaFree;
        std::optional<std::uint64_t> hugePagesTotal;
        std::optional<std::uint64_t> hugePagesFree;
        std::optional<std::uint64_t> hugePagesReserved;
        std::optional<std::uint64_t> hugePagesSurplus;
        std::optional<std::uint64_t> hugePageSize;
        std::optional<std::uint64_t> directMap4K;
        std::optional<std::uint64_t> directMap4M;
        std::optional<std::uint64_t> directMap2M;
        std::optional<std::uint64_t> directMap1G;
        std::optional<std::uint64_t> hugeTLB;
        std::optional<std::uint64_t> perCPU;
        std::optional<std::uint64_t> reclaimableKernel;
        std::optional<std::uint64_t> filePMDMapped;
        std::optional<std::uint64_t> fileHugePages;
        std::optional<std::uint64_t> zSwap;
        std::optional<std::uint64_t> zSwapped;
    };

    std::expected<MemoryStat, std::error_code> memory();
}

DECLARE_ERROR_CODE(zero::os::linux::procfs::Error)

#endif //ZERO_PROCFS_H
