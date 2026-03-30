#include <zero/os/linux/procfs/procfs.h>
#include <zero/meta/type_traits.h>
#include <zero/strings.h>
#include <zero/filesystem.h>
#include <zero/expect.h>
#include <map>

namespace {
    std::expected<zero::os::linux::procfs::CPUTime, std::error_code> parseCPUTime(const std::string_view str) {
        const auto tokens = zero::strings::split(str);

        if (tokens.size() < 5)
            return std::unexpected{zero::os::linux::procfs::Error::UnexpectedData};

        auto it = tokens.begin() + 1;

        const auto set = [&]<typename T>(T &var) -> std::expected<void, std::error_code> {
            if constexpr (zero::meta::IsSpecialization<T, std::optional>) {
                if (it == tokens.end())
                    return {};

                const auto value = zero::strings::toNumber<typename T::value_type>(*it++);
                Z_EXPECT(value);
                var = *value;
            }
            else {
                const auto value = zero::strings::toNumber<T>(*it++);
                Z_EXPECT(value);
                var = *value;
            }

            return {};
        };

        zero::os::linux::procfs::CPUTime cpuTime;

        Z_EXPECT(set(cpuTime.user));
        Z_EXPECT(set(cpuTime.nice));
        Z_EXPECT(set(cpuTime.system));
        Z_EXPECT(set(cpuTime.idle));
        Z_EXPECT(set(cpuTime.ioWait));
        Z_EXPECT(set(cpuTime.irq));
        Z_EXPECT(set(cpuTime.softIRQ));
        Z_EXPECT(set(cpuTime.steal));
        Z_EXPECT(set(cpuTime.guest));
        Z_EXPECT(set(cpuTime.guestNice));

        return cpuTime;
    }
}

std::expected<zero::os::linux::procfs::Stat, std::error_code> zero::os::linux::procfs::stat() {
    const auto content = filesystem::readString("/proc/stat");
    Z_EXPECT(content);

    Stat stat;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        if (line.starts_with("cpu ")) {
            const auto cpuTime = parseCPUTime(line);
            Z_EXPECT(cpuTime);
            stat.total = *cpuTime;
        }
        else if (line.starts_with("cpu")) {
            const auto cpuTime = parseCPUTime(line);
            Z_EXPECT(cpuTime);
            stat.cpuTimes.push_back(*cpuTime);
        }
        else if (line.starts_with("ctxt ")) {
            const auto value = strings::toNumber<std::uint64_t>(line.substr(5));
            Z_EXPECT(value);
            stat.contextSwitches = *value;
        }
        else if (line.starts_with("btime ")) {
            const auto value = strings::toNumber<std::uint64_t>(line.substr(6));
            Z_EXPECT(value);
            stat.bootTime = *value;
        }
        else if (line.starts_with("processes ")) {
            const auto value = strings::toNumber<std::uint64_t>(line.substr(10));
            Z_EXPECT(value);
            stat.processesCreated = *value;
        }
        else if (line.starts_with("procs_running ")) {
            const auto value = strings::toNumber<std::uint32_t>(line.substr(14));
            Z_EXPECT(value);
            stat.processesRunning = *value;
        }
        else if (line.starts_with("procs_blocked ")) {
            const auto value = strings::toNumber<std::uint32_t>(line.substr(14));
            Z_EXPECT(value);
            stat.processesBlocked = *value;
        }
    }

    return stat;
}

std::expected<zero::os::linux::procfs::MemoryStat, std::error_code> zero::os::linux::procfs::memory() {
    const auto content = filesystem::readString("/proc/meminfo");
    Z_EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected{Error::UnexpectedData};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto set = [&]<typename T>(T &var, const std::string &key) -> std::expected<void, std::error_code> {
        const auto it = map.find(key);

        if constexpr (zero::meta::IsSpecialization<T, std::optional>) {
            if (it == map.end())
                return {};

            const auto value = strings::toNumber<typename T::value_type>(it->second);
            Z_EXPECT(value);
            var = *value;
        }
        else {
            if (it == map.end())
                return std::unexpected{Error::UnexpectedData};

            const auto value = strings::toNumber<T>(it->second);
            Z_EXPECT(value);
            var = *value;
        }

        return {};
    };

    MemoryStat stat;

    Z_EXPECT(set(stat.memoryTotal, "MemTotal"));
    Z_EXPECT(set(stat.memoryFree, "MemFree"));
    Z_EXPECT(set(stat.memoryAvailable, "MemAvailable"));
    Z_EXPECT(set(stat.buffers, "Buffers"));
    Z_EXPECT(set(stat.cached, "Cached"));
    Z_EXPECT(set(stat.swapCached, "SwapCached"));
    Z_EXPECT(set(stat.active, "Active"));
    Z_EXPECT(set(stat.inactive, "Inactive"));
    Z_EXPECT(set(stat.activeAnonymous, "Active(anon)"));
    Z_EXPECT(set(stat.inactiveAnonymous, "Inactive(anon)"));
    Z_EXPECT(set(stat.activeFile, "Active(file)"));
    Z_EXPECT(set(stat.inactiveFile, "Inactive(file)"));
    Z_EXPECT(set(stat.unevictable, "Unevictable"));
    Z_EXPECT(set(stat.mLocked, "Mlocked"));
    Z_EXPECT(set(stat.highTotal, "HighTotal"));
    Z_EXPECT(set(stat.highFree, "HighFree"));
    Z_EXPECT(set(stat.lowTotal, "LowTotal"));
    Z_EXPECT(set(stat.lowFree, "LowFree"));
    Z_EXPECT(set(stat.mmapCopy, "MmapCopy"));
    Z_EXPECT(set(stat.swapTotal, "SwapTotal"));
    Z_EXPECT(set(stat.swapFree, "SwapFree"));
    Z_EXPECT(set(stat.dirty, "Dirty"));
    Z_EXPECT(set(stat.writeBack, "Writeback"));
    Z_EXPECT(set(stat.anonymousPages, "AnonPages"));
    Z_EXPECT(set(stat.mapped, "Mapped"));
    Z_EXPECT(set(stat.sharedMemory, "Shmem"));
    Z_EXPECT(set(stat.slab, "Slab"));
    Z_EXPECT(set(stat.reclaimableSlab, "SReclaimable"));
    Z_EXPECT(set(stat.unreclaimableSlab, "SUnreclaim"));
    Z_EXPECT(set(stat.kernelStack, "KernelStack"));
    Z_EXPECT(set(stat.pageTables, "PageTables"));
    Z_EXPECT(set(stat.secondaryPageTables, "SecPageTables"));
    Z_EXPECT(set(stat.quickLists, "Quicklists"));
    Z_EXPECT(set(stat.nfsUnstable, "NFS_Unstable"));
    Z_EXPECT(set(stat.bounceBuffers, "Bounce"));
    Z_EXPECT(set(stat.writeBackTemporary, "WritebackTmp"));
    Z_EXPECT(set(stat.commitLimit, "CommitLimit"));
    Z_EXPECT(set(stat.committedAs, "Committed_AS"));
    Z_EXPECT(set(stat.vmallocTotal, "VmallocTotal"));
    Z_EXPECT(set(stat.vmallocUsed, "VmallocUsed"));
    Z_EXPECT(set(stat.vmallocChunk, "VmallocChunk"));
    Z_EXPECT(set(stat.hardwareCorrupted, "HardwareCorrupted"));
    Z_EXPECT(set(stat.anonymousHugePages, "AnonHugePages"));
    Z_EXPECT(set(stat.sharedMemoryHugePages, "ShmemHugePages"));
    Z_EXPECT(set(stat.sharedMemoryPMDMapped, "ShmemPmdMapped"));
    Z_EXPECT(set(stat.cmaTotal, "CmaTotal"));
    Z_EXPECT(set(stat.cmaFree, "CmaFree"));
    Z_EXPECT(set(stat.hugePagesTotal, "HugePages_Total"));
    Z_EXPECT(set(stat.hugePagesFree, "HugePages_Free"));
    Z_EXPECT(set(stat.hugePagesReserved, "HugePages_Rsvd"));
    Z_EXPECT(set(stat.hugePagesSurplus, "HugePages_Surp"));
    Z_EXPECT(set(stat.hugePageSize, "Hugepagesize"));
    Z_EXPECT(set(stat.directMap4K, "DirectMap4k"));
    Z_EXPECT(set(stat.directMap4M, "DirectMap4M"));
    Z_EXPECT(set(stat.directMap2M, "DirectMap2M"));
    Z_EXPECT(set(stat.directMap1G, "DirectMap1G"));
    Z_EXPECT(set(stat.hugeTLB, "Hugetlb"));
    Z_EXPECT(set(stat.perCPU, "Percpu"));
    Z_EXPECT(set(stat.reclaimableKernel, "KReclaimable"));
    Z_EXPECT(set(stat.filePMDMapped, "FilePmdMapped"));
    Z_EXPECT(set(stat.fileHugePages, "FileHugePages"));
    Z_EXPECT(set(stat.zSwap, "Zswap"));
    Z_EXPECT(set(stat.zSwapped, "Zswapped"));

    return stat;
}

Z_DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::linux::procfs::Error)
