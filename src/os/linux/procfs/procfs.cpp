#include <zero/os/linux/procfs/procfs.h>
#include <zero/meta/type_traits.h>
#include <zero/error.h>
#include <zero/strings.h>
#include <zero/filesystem.h>
#include <fmt/format.h>
#include <map>

namespace {
    zero::os::linux::procfs::CPUTime parseCPUTime(const std::string_view str) {
        const auto tokens = zero::strings::split(str);

        if (tokens.size() < 5)
            throw zero::error::StacktraceError<std::runtime_error>{
                fmt::format("Failed to parse CPU time from /proc/stat: expected >= 5 fields, got {}", tokens.size())
            };

        auto it = tokens.begin() + 1;

        const auto set = [&]<typename T>(T &var) {
            if constexpr (zero::meta::IsSpecialization<T, std::optional>) {
                if (it == tokens.end())
                    return;

                var = zero::error::guard(zero::strings::toNumber<typename T::value_type>(*it++));
            }
            else {
                var = zero::error::guard(zero::strings::toNumber<T>(*it++));
            }
        };

        zero::os::linux::procfs::CPUTime cpuTime;

        set(cpuTime.user);
        set(cpuTime.nice);
        set(cpuTime.system);
        set(cpuTime.idle);
        set(cpuTime.ioWait);
        set(cpuTime.irq);
        set(cpuTime.softIRQ);
        set(cpuTime.steal);
        set(cpuTime.guest);
        set(cpuTime.guestNice);

        return cpuTime;
    }
}

zero::os::linux::procfs::Stat zero::os::linux::procfs::stat() {
    Stat stat;
    const auto content = error::guard(filesystem::readString("/proc/stat"));

    for (const auto &line: strings::split(strings::trim(content), "\n")) {
        if (line.starts_with("cpu "))
            stat.total = parseCPUTime(line);
        else if (line.starts_with("cpu"))
            stat.cpuTimes.push_back(parseCPUTime(line));
        else if (line.starts_with("ctxt "))
            stat.contextSwitches = error::guard(strings::toNumber<std::uint64_t>(line.substr(5)));
        else if (line.starts_with("btime "))
            stat.bootTime = error::guard(strings::toNumber<std::uint64_t>(line.substr(6)));
        else if (line.starts_with("processes "))
            stat.processesCreated = error::guard(strings::toNumber<std::uint64_t>(line.substr(10)));
        else if (line.starts_with("procs_running "))
            stat.processesRunning = error::guard(strings::toNumber<std::uint32_t>(line.substr(14)));
        else if (line.starts_with("procs_blocked "))
            stat.processesBlocked = error::guard(strings::toNumber<std::uint32_t>(line.substr(14)));
    }

    return stat;
}

zero::os::linux::procfs::MemoryStat zero::os::linux::procfs::memory() {
    std::map<std::string, std::string> map;
    const auto content = error::guard(filesystem::readString("/proc/meminfo"));

    for (const auto &line: strings::split(strings::trim(content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            throw error::StacktraceError<std::runtime_error>{"Malformed line in /proc/meminfo: missing ':' separator"};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto set = [&]<typename T>(T &var, const std::string &key) {
        const auto it = map.find(key);

        if constexpr (zero::meta::IsSpecialization<T, std::optional>) {
            if (it == map.end())
                return;

            var = error::guard(strings::toNumber<typename T::value_type>(it->second));
        }
        else {
            if (it == map.end())
                throw error::StacktraceError<std::runtime_error>{
                    fmt::format("Missing required field '{}' in /proc/meminfo", key)
                };

            var = error::guard(strings::toNumber<T>(it->second));
        }
    };

    MemoryStat stat;

    set(stat.memoryTotal, "MemTotal");
    set(stat.memoryFree, "MemFree");
    set(stat.memoryAvailable, "MemAvailable");
    set(stat.buffers, "Buffers");
    set(stat.cached, "Cached");
    set(stat.swapCached, "SwapCached");
    set(stat.active, "Active");
    set(stat.inactive, "Inactive");
    set(stat.activeAnonymous, "Active(anon)");
    set(stat.inactiveAnonymous, "Inactive(anon)");
    set(stat.activeFile, "Active(file)");
    set(stat.inactiveFile, "Inactive(file)");
    set(stat.unevictable, "Unevictable");
    set(stat.mLocked, "Mlocked");
    set(stat.highTotal, "HighTotal");
    set(stat.highFree, "HighFree");
    set(stat.lowTotal, "LowTotal");
    set(stat.lowFree, "LowFree");
    set(stat.mmapCopy, "MmapCopy");
    set(stat.swapTotal, "SwapTotal");
    set(stat.swapFree, "SwapFree");
    set(stat.dirty, "Dirty");
    set(stat.writeBack, "Writeback");
    set(stat.anonymousPages, "AnonPages");
    set(stat.mapped, "Mapped");
    set(stat.sharedMemory, "Shmem");
    set(stat.slab, "Slab");
    set(stat.reclaimableSlab, "SReclaimable");
    set(stat.unreclaimableSlab, "SUnreclaim");
    set(stat.kernelStack, "KernelStack");
    set(stat.pageTables, "PageTables");
    set(stat.secondaryPageTables, "SecPageTables");
    set(stat.quickLists, "Quicklists");
    set(stat.nfsUnstable, "NFS_Unstable");
    set(stat.bounceBuffers, "Bounce");
    set(stat.writeBackTemporary, "WritebackTmp");
    set(stat.commitLimit, "CommitLimit");
    set(stat.committedAs, "Committed_AS");
    set(stat.vmallocTotal, "VmallocTotal");
    set(stat.vmallocUsed, "VmallocUsed");
    set(stat.vmallocChunk, "VmallocChunk");
    set(stat.hardwareCorrupted, "HardwareCorrupted");
    set(stat.anonymousHugePages, "AnonHugePages");
    set(stat.sharedMemoryHugePages, "ShmemHugePages");
    set(stat.sharedMemoryPMDMapped, "ShmemPmdMapped");
    set(stat.cmaTotal, "CmaTotal");
    set(stat.cmaFree, "CmaFree");
    set(stat.hugePagesTotal, "HugePages_Total");
    set(stat.hugePagesFree, "HugePages_Free");
    set(stat.hugePagesReserved, "HugePages_Rsvd");
    set(stat.hugePagesSurplus, "HugePages_Surp");
    set(stat.hugePageSize, "Hugepagesize");
    set(stat.directMap4K, "DirectMap4k");
    set(stat.directMap4M, "DirectMap4M");
    set(stat.directMap2M, "DirectMap2M");
    set(stat.directMap1G, "DirectMap1G");
    set(stat.hugeTLB, "Hugetlb");
    set(stat.perCPU, "Percpu");
    set(stat.reclaimableKernel, "KReclaimable");
    set(stat.filePMDMapped, "FilePmdMapped");
    set(stat.fileHugePages, "FileHugePages");
    set(stat.zSwap, "Zswap");
    set(stat.zSwapped, "Zswapped");

    return stat;
}
