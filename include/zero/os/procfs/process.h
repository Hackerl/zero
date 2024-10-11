#ifndef ZERO_PROCFS_PROCESS_H
#define ZERO_PROCFS_PROCESS_H

#include <map>
#include <list>
#include <array>
#include <vector>
#include <bitset>
#include <optional>
#include <expected>
#include <filesystem>
#include <zero/error.h>

namespace zero::os::procfs::process {
    enum MemoryPermission {
        READ,
        WRITE,
        EXECUTE,
        SHARED,
        PRIVATE
    };

    struct MemoryMapping {
        std::uint64_t start;
        std::uint64_t end;
        std::bitset<5> permissions;
        std::uint64_t offset;
        std::string device;
        std::uint64_t inode;
        std::optional<std::string> pathname;
    };

    struct StatM {
        std::uint64_t totalSize;
        std::uint64_t residentSetSize;
        std::uint64_t sharedPages;
        std::uint64_t textSegmentSize;
        std::uint64_t librarySize;
        std::uint64_t dataAndStackSize;
        std::uint64_t dirtyPages;
    };

    struct Stat {
        std::int32_t pid;
        std::string comm;
        char state;
        std::int32_t ppid;
        std::int32_t processGroupID;
        std::int32_t sessionID;
        std::int32_t ttyNumber;
        std::int32_t terminalProcessGroupID;
        std::uint32_t flags;
        std::uint64_t minorFaults;
        std::uint64_t childMinorFaults;
        std::uint64_t majorFaults;
        std::uint64_t childMajorFaults;
        std::uint64_t userTime;
        std::uint64_t systemTime;
        std::int64_t childUserTime;
        std::int64_t childSystemTime;
        std::int64_t priority;
        std::int64_t niceValue;
        std::int64_t numThreads;
        std::int64_t intervalRealValue;
        std::uint64_t startTime;
        std::uint64_t virtualMemorySize;
        std::uint64_t rss;
        std::uint64_t rssLimit;
        std::uint64_t startCode;
        std::uint64_t endCode;
        std::uint64_t startStack;
        std::uint64_t kernelStackPointer;
        std::uint64_t kernelInstructionPointer;
        std::uint64_t pendingSignals;
        std::uint64_t blockedSignals;
        std::uint64_t ignoredSignals;
        std::uint64_t caughtSignals;
        std::uint64_t waitingChannel;
        std::uint64_t pagesSwapped;
        std::uint64_t childPagesSwapped;
        std::optional<std::int32_t> exitSignal;
        std::optional<std::int32_t> processor;
        std::optional<std::uint32_t> realTimePriority;
        std::optional<std::uint32_t> schedulingPolicy;
        std::optional<std::uint64_t> blockIODelayTicks;
        std::optional<std::uint64_t> guestTime;
        std::optional<std::int64_t> childGuestTime;
        std::optional<std::uint64_t> startData;
        std::optional<std::uint64_t> endData;
        std::optional<std::uint64_t> startBrk;
        std::optional<std::uint64_t> argStart;
        std::optional<std::uint64_t> argEnd;
        std::optional<std::uint64_t> envStart;
        std::optional<std::uint64_t> envEnd;
        std::optional<std::int64_t> exitCode;
    };

    struct Status {
        std::string name;
        std::optional<std::uint32_t> umask;
        std::string state;
        std::int32_t threadGroupID;
        std::optional<std::int32_t> numaGroupID;
        std::int32_t pid;
        std::int32_t ppid;
        std::int32_t tracerPID;
        std::array<std::uint32_t, 4> uid;
        std::array<std::uint32_t, 4> gid;
        std::uint32_t fdSize;
        std::vector<std::int32_t> supplementaryGroupIDs;
        std::optional<std::vector<std::int32_t>> namespaceThreadGroupIDs;
        std::optional<std::vector<std::int32_t>> namespaceProcessIDs;
        std::optional<std::vector<std::int32_t>> namespaceProcessGroupIDs;
        std::optional<std::vector<std::int32_t>> namespaceSessionIDs;
        std::optional<std::uint64_t> vmPeak;
        std::optional<std::uint64_t> vmSize;
        std::optional<std::uint64_t> vmLocked;
        std::optional<std::uint64_t> vmPinned;
        std::optional<std::uint64_t> vmHWM;
        std::optional<std::uint64_t> vmRSS;
        std::optional<std::uint64_t> rssAnonymous;
        std::optional<std::uint64_t> rssFile;
        std::optional<std::uint64_t> rssSharedMemory;
        std::optional<std::uint64_t> vmData;
        std::optional<std::uint64_t> vmStack;
        std::optional<std::uint64_t> vmExe;
        std::optional<std::uint64_t> vmLib;
        std::optional<std::uint64_t> vmPTE;
        std::optional<std::uint64_t> vmSwap;
        std::optional<std::uint64_t> hugeTLBPages;
        std::uint64_t threads;
        std::array<std::uint64_t, 2> signalQueue;
        std::uint64_t pendingSignals;
        std::uint64_t sharedPendingSignals;
        std::uint64_t blockedSignals;
        std::uint64_t ignoredSignals;
        std::uint64_t caughtSignals;
        std::uint64_t inheritableCapabilities;
        std::uint64_t permittedCapabilities;
        std::uint64_t effectiveCapabilities;
        std::optional<std::uint64_t> boundingCapabilities;
        std::optional<std::uint64_t> ambientCapabilities;
        std::optional<std::uint64_t> noNewPrivileges;
        std::optional<std::uint32_t> seccompMode;
        std::optional<std::string> speculationStoreBypass;
        std::optional<std::vector<std::uint32_t>> allowedCpus;
        std::optional<std::vector<std::pair<std::uint32_t, std::uint32_t>>> allowedCpuList;
        std::optional<std::vector<std::uint32_t>> allowedMemoryNodes;
        std::optional<std::vector<std::pair<std::uint32_t, std::uint32_t>>> allowedMemoryNodeList;
        std::optional<std::uint64_t> voluntaryContextSwitches;
        std::optional<std::uint64_t> nonVoluntaryContextSwitches;
        std::optional<bool> coreDumping;
        std::optional<bool> thpEnabled;
    };

    struct IOStat {
        std::uint64_t readCharacters;
        std::uint64_t writeCharacters;
        std::uint64_t readSyscalls;
        std::uint64_t writeSyscalls;
        std::uint64_t readBytes;
        std::uint64_t writeBytes;
        std::uint64_t cancelledWriteBytes;
    };

    class Process {
    public:
        DEFINE_ERROR_CODE_INNER(
            Error,
            "zero::os::procfs::process::Process",
            MAYBE_ZOMBIE_PROCESS, "maybe zombie process"
        )

        Process(int fd, pid_t pid);
        Process(Process &&rhs) noexcept;
        Process &operator=(Process &&rhs) noexcept;
        ~Process();

    private:
        [[nodiscard]] std::expected<std::string, std::error_code> readFile(const char *filename) const;

    public:
        [[nodiscard]] pid_t pid() const;

        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] std::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] std::expected<std::map<std::string, std::string>, std::error_code> environ() const;

        [[nodiscard]] std::expected<Stat, std::error_code> stat() const;
        [[nodiscard]] std::expected<StatM, std::error_code> statM() const;
        [[nodiscard]] std::expected<Status, std::error_code> status() const;
        [[nodiscard]] std::expected<std::list<pid_t>, std::error_code> tasks() const;
        [[nodiscard]] std::expected<std::list<MemoryMapping>, std::error_code> maps() const;
        [[nodiscard]] std::expected<IOStat, std::error_code> io() const;

    private:
        int mFD;
        pid_t mPID;
    };

    std::expected<Process, std::error_code> self();
    std::expected<Process, std::error_code> open(pid_t pid);
    std::expected<std::list<pid_t>, std::error_code> all();
}

DECLARE_ERROR_CODE(zero::os::procfs::process::Process::Error)

#endif //ZERO_PROCFS_PROCESS_H
