#include <zero/os/linux/procfs/procfs.h>
#include <zero/detail/type_traits.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/fs.h>
#include <zero/expect.h>
#include <unistd.h>
#include <map>

std::expected<zero::os::linux::procfs::CPUTime, std::error_code> parseCPUTime(const std::string_view str) {
    const auto tokens = zero::strings::split(str);

    if (tokens.size() < 5)
        return std::unexpected{zero::os::linux::procfs::Error::UNEXPECTED_DATA};

    auto it = tokens.begin() + 1;

    const auto set = [&]<typename T>(T &var) -> std::expected<void, std::error_code> {
        if constexpr (zero::detail::is_specialization_v<T, std::optional>) {
            if (it == tokens.end())
                return {};

            const auto value = zero::strings::toNumber<typename T::value_type>(*it++);
            EXPECT(value);
            var = *value;
        }
        else {
            const auto value = zero::strings::toNumber<T>(*it++);
            EXPECT(value);
            var = *value;
        }

        return {};
    };

    zero::os::linux::procfs::CPUTime cpuTime;

    EXPECT(set(cpuTime.user));
    EXPECT(set(cpuTime.nice));
    EXPECT(set(cpuTime.system));
    EXPECT(set(cpuTime.idle));
    EXPECT(set(cpuTime.ioWait));
    EXPECT(set(cpuTime.irq));
    EXPECT(set(cpuTime.softIRQ));
    EXPECT(set(cpuTime.steal));
    EXPECT(set(cpuTime.guest));
    EXPECT(set(cpuTime.guestNice));

    return cpuTime;
}

std::expected<zero::os::linux::procfs::Stat, std::error_code> zero::os::linux::procfs::stat() {
    const auto content = filesystem::readString("/proc/stat");
    EXPECT(content);

    Stat stat;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        if (line.starts_with("cpu ")) {
            const auto cpuTime = parseCPUTime(line);
            EXPECT(cpuTime);
            stat.total = *cpuTime;
        }
        else if (line.starts_with("cpu")) {
            const auto cpuTime = parseCPUTime(line);
            EXPECT(cpuTime);
            stat.cpuTimes.push_back(*cpuTime);
        }
        else if (line.starts_with("ctxt ")) {
            const auto value = strings::toNumber<std::uint64_t>(line.substr(5));
            EXPECT(value);
            stat.contextSwitches = *value;
        }
        else if (line.starts_with("btime ")) {
            const auto value = strings::toNumber<std::uint64_t>(line.substr(6));
            EXPECT(value);
            stat.bootTime = *value;
        }
        else if (line.starts_with("processes ")) {
            const auto value = strings::toNumber<std::uint64_t>(line.substr(10));
            EXPECT(value);
            stat.processesCreated = *value;
        }
        else if (line.starts_with("procs_running ")) {
            const auto value = strings::toNumber<std::uint32_t>(line.substr(14));
            EXPECT(value);
            stat.processesRunning = *value;
        }
        else if (line.starts_with("procs_blocked ")) {
            const auto value = strings::toNumber<std::uint32_t>(line.substr(14));
            EXPECT(value);
            stat.processesBlocked = *value;
        }
    }

    return stat;
}

std::expected<zero::os::linux::procfs::MemoryStat, std::error_code> zero::os::linux::procfs::memory() {
    const auto content = filesystem::readString("/proc/meminfo");
    EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected{Error::UNEXPECTED_DATA};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto set = [&]<typename T>(T &var, const std::string &key) -> std::expected<void, std::error_code> {
        const auto it = map.find(key);

        if constexpr (zero::detail::is_specialization_v<T, std::optional>) {
            if (it == map.end())
                return {};

            const auto value = strings::toNumber<typename T::value_type>(it->second);
            EXPECT(value);
            var = *value;
        }
        else {
            if (it == map.end())
                return std::unexpected{Error::UNEXPECTED_DATA};

            const auto value = strings::toNumber<T>(it->second);
            EXPECT(value);
            var = *value;
        }

        return {};
    };

    MemoryStat stat;

    EXPECT(set(stat.memoryTotal, "MemTotal"));
    EXPECT(set(stat.memoryFree, "MemFree"));
    EXPECT(set(stat.memoryAvailable, "MemAvailable"));
    EXPECT(set(stat.buffers, "Buffers"));
    EXPECT(set(stat.cached, "Cached"));
    EXPECT(set(stat.swapCached, "SwapCached"));
    EXPECT(set(stat.active, "Active"));
    EXPECT(set(stat.inactive, "Inactive"));
    EXPECT(set(stat.activeAnonymous, "Active(anon)"));
    EXPECT(set(stat.inactiveAnonymous, "Inactive(anon)"));
    EXPECT(set(stat.activeFile, "Active(file)"));
    EXPECT(set(stat.inactiveFile, "Inactive(file)"));
    EXPECT(set(stat.unevictable, "Unevictable"));
    EXPECT(set(stat.mLocked, "Mlocked"));
    EXPECT(set(stat.highTotal, "HighTotal"));
    EXPECT(set(stat.highFree, "HighFree"));
    EXPECT(set(stat.lowTotal, "LowTotal"));
    EXPECT(set(stat.lowFree, "LowFree"));
    EXPECT(set(stat.mmapCopy, "MmapCopy"));
    EXPECT(set(stat.swapTotal, "SwapTotal"));
    EXPECT(set(stat.swapFree, "SwapFree"));
    EXPECT(set(stat.dirty, "Dirty"));
    EXPECT(set(stat.writeBack, "Writeback"));
    EXPECT(set(stat.anonymousPages, "AnonPages"));
    EXPECT(set(stat.mapped, "Mapped"));
    EXPECT(set(stat.sharedMemory, "Shmem"));
    EXPECT(set(stat.slab, "Slab"));
    EXPECT(set(stat.reclaimableSlab, "SReclaimable"));
    EXPECT(set(stat.unreclaimableSlab, "SUnreclaim"));
    EXPECT(set(stat.kernelStack, "KernelStack"));
    EXPECT(set(stat.pageTables, "PageTables"));
    EXPECT(set(stat.secondaryPageTables, "SecPageTables"));
    EXPECT(set(stat.quickLists, "Quicklists"));
    EXPECT(set(stat.nfsUnstable, "NFS_Unstable"));
    EXPECT(set(stat.bounceBuffers, "Bounce"));
    EXPECT(set(stat.writeBackTemporary, "WritebackTmp"));
    EXPECT(set(stat.commitLimit, "CommitLimit"));
    EXPECT(set(stat.committedAs, "Committed_AS"));
    EXPECT(set(stat.vmallocTotal, "VmallocTotal"));
    EXPECT(set(stat.vmallocUsed, "VmallocUsed"));
    EXPECT(set(stat.vmallocChunk, "VmallocChunk"));
    EXPECT(set(stat.hardwareCorrupted, "HardwareCorrupted"));
    EXPECT(set(stat.anonymousHugePages, "AnonHugePages"));
    EXPECT(set(stat.sharedMemoryHugePages, "ShmemHugePages"));
    EXPECT(set(stat.sharedMemoryPMDMapped, "ShmemPmdMapped"));
    EXPECT(set(stat.cmaTotal, "CmaTotal"));
    EXPECT(set(stat.cmaFree, "CmaFree"));
    EXPECT(set(stat.hugePagesTotal, "HugePages_Total"));
    EXPECT(set(stat.hugePagesFree, "HugePages_Free"));
    EXPECT(set(stat.hugePagesReserved, "HugePages_Rsvd"));
    EXPECT(set(stat.hugePagesSurplus, "HugePages_Surp"));
    EXPECT(set(stat.hugePageSize, "Hugepagesize"));
    EXPECT(set(stat.directMap4K, "DirectMap4k"));
    EXPECT(set(stat.directMap4M, "DirectMap4M"));
    EXPECT(set(stat.directMap2M, "DirectMap2M"));
    EXPECT(set(stat.directMap1G, "DirectMap1G"));
    EXPECT(set(stat.hugeTLB, "Hugetlb"));
    EXPECT(set(stat.perCPU, "Percpu"));
    EXPECT(set(stat.reclaimableKernel, "KReclaimable"));
    EXPECT(set(stat.filePMDMapped, "FilePmdMapped"));
    EXPECT(set(stat.fileHugePages, "FileHugePages"));
    EXPECT(set(stat.zSwap, "Zswap"));
    EXPECT(set(stat.zSwapped, "Zswapped"));

    return stat;
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::linux::procfs::Error)
