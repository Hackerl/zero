#ifndef ZERO_PROCFS_PROCESS_H
#define ZERO_PROCFS_PROCESS_H

#include <map>
#include <list>
#include <vector>
#include <optional>
#include <filesystem>
#include <tl/expected.hpp>
#include <sys/types.h>

namespace zero::os::procfs::process {
    enum MemoryPermission {
        READ = 0x1,
        WRITE = 0x2,
        EXECUTE = 0x4,
        SHARED = 0x8,
        PRIVATE = 0x10
    };

    struct MemoryMapping {
        std::uintptr_t start;
        std::uintptr_t end;
        int permissions;
        off_t offset;
        std::string device;
        ino_t inode;
        std::string pathname;
    };

    struct StatM {
        std::uint64_t size;
        std::uint64_t resident;
        std::uint64_t shared;
        std::uint64_t text;
        std::uint64_t library;
        std::uint64_t data;
        std::uint64_t dirty;
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

    struct Status {
        std::string name;
        std::optional<mode_t> umask;
        std::string state;
        pid_t tgid;
        std::optional<pid_t> ngid;
        pid_t pid;
        pid_t ppid;
        pid_t tracerPID;
        uid_t uid[4];
        pid_t gid[4];
        int fdSize;
        std::vector<pid_t> groups;
        std::optional<pid_t> nstgid;
        std::optional<pid_t> nspid;
        std::optional<pid_t> nspgid;
        std::optional<int> nssid;
        std::optional<unsigned long> vmPeak;
        std::optional<unsigned long> vmSize;
        std::optional<unsigned long> vmLck;
        std::optional<unsigned long> vmPin;
        std::optional<unsigned long> vmHWM;
        std::optional<unsigned long> vmRSS;
        std::optional<unsigned long> rssAnon;
        std::optional<unsigned long> rssFile;
        std::optional<unsigned long> rssShMem;
        std::optional<unsigned long> vmData;
        std::optional<unsigned long> vmStk;
        std::optional<unsigned long> vmExe;
        std::optional<unsigned long> vmLib;
        std::optional<unsigned long> vmPTE;
        std::optional<unsigned long> vmPMD;
        std::optional<unsigned long> vmSwap;
        std::optional<unsigned long> hugeTLBPages;
        int threads;
        int sigQ[2];
        unsigned long sigPnd;
        unsigned long shdPnd;
        unsigned long sigBlk;
        unsigned long sigIgn;
        unsigned long sigCgt;
        unsigned long capInh;
        unsigned long capPrm;
        unsigned long capEff;
        std::optional<unsigned long> capBnd;
        std::optional<unsigned long> capAmb;
        std::optional<unsigned long> noNewPrivileges;
        std::optional<int> seccomp;
        std::optional<std::string> speculationStoreBypass;
        std::optional<std::vector<unsigned int>> cpusAllowed;
        std::optional<std::vector<std::pair<unsigned int, unsigned int>>> cpusAllowedList;
        std::optional<std::vector<unsigned int>> memoryNodesAllowed;
        std::optional<std::vector<std::pair<unsigned int, unsigned int>>> memoryNodesAllowedList;
        std::optional<int> voluntaryContextSwitches;
        std::optional<int> nonVoluntaryContextSwitches;
        std::optional<bool> coreDumping;
        std::optional<bool> thpEnabled;
    };

    struct IOStat {
        unsigned long long readCharacters;
        unsigned long long writeCharacters;
        unsigned long long readSyscall;
        unsigned long long writeSyscall;
        unsigned long long readBytes;
        unsigned long long writeBytes;
        unsigned long long cancelledWriteBytes;
    };

    class Process {
    public:
        Process(int fd, pid_t pid);
        Process(Process &&rhs) noexcept;
        ~Process();

    private:
        [[nodiscard]] tl::expected<std::string, std::error_code> readFile(const char *filename) const;

    public:
        [[nodiscard]] pid_t pid() const;

        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> environ() const;

        [[nodiscard]] tl::expected<Stat, std::error_code> stat() const;
        [[nodiscard]] tl::expected<StatM, std::error_code> statM() const;
        [[nodiscard]] tl::expected<Status, std::error_code> status() const;
        [[nodiscard]] tl::expected<std::list<pid_t>, std::error_code> tasks() const;
        [[nodiscard]] tl::expected<std::list<MemoryMapping>, std::error_code> maps() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

    private:
        int mFD;
        pid_t mPID;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(pid_t pid);
    tl::expected<std::list<pid_t>, std::error_code> all();
}

#endif //ZERO_PROCFS_PROCESS_H
