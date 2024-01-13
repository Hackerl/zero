#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <array>

#ifdef _WIN32
#include "nt/process.h"
#elif __APPLE__
#include "darwin/process.h"
#elif __linux__
#include "procfs/process.h"
#endif

namespace zero::os::process {
#ifdef _WIN32
    using Process = nt::process::Process;
    using CPUTime = nt::process::CPUTime;
    using MemoryStat = nt::process::MemoryStat;
    using IOStat = nt::process::IOStat;

    constexpr auto all = nt::process::all;
    constexpr auto self = nt::process::self;
    constexpr auto open = nt::process::open;
#elif __APPLE__
    using Process = darwin::process::Process;
    using CPUTime = darwin::process::CPUTime;
    using MemoryStat = darwin::process::MemoryStat;
    using IOStat = darwin::process::IOStat;

    constexpr auto all = darwin::process::all;
    constexpr auto self = darwin::process::self;
    constexpr auto open = darwin::process::open;
#elif __linux__
    constexpr auto all = procfs::process::all;

    struct CPUTime {
        double user{};
        double system{};
        std::optional<double> ioWait;
    };

    struct MemoryStat {
        std::uint64_t rss;
        std::uint64_t vms;
    };

    using IOStat = procfs::process::IOStat;

    class Process {
    public:
        explicit Process(procfs::process::Process impl);

        procfs::process::Process &impl();
        [[nodiscard]] const procfs::process::Process &impl() const;

        [[nodiscard]] pid_t pid() const;
        [[nodiscard]] tl::expected<pid_t, std::error_code> ppid() const;

        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> envs() const;

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

        tl::expected<void, std::error_code> kill(int sig);

    private:
        procfs::process::Process mImpl;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(pid_t pid);
#endif

    class ChildProcess final : public Process {
    public:
#ifdef _WIN32
        using StdioFile = HANDLE;
#else
        using StdioFile = int;
#endif
        ChildProcess(Process process, const std::array<std::optional<StdioFile>, 3> &stdio);
        ChildProcess(ChildProcess &&rhs) noexcept;
        ~ChildProcess();

        std::optional<StdioFile> &stdInput();
        std::optional<StdioFile> &stdOutput();
        std::optional<StdioFile> &stdError();

#ifndef _WIN32
        [[nodiscard]] tl::expected<int, std::error_code> wait() const;
        [[nodiscard]] tl::expected<int, std::error_code> tryWait() const;
#endif

    private:
        std::array<std::optional<StdioFile>, 3> mStdio;
    };

    struct Output {
#ifdef _WIN32
        DWORD status;
#else
        int status;
#endif
        std::vector<std::byte> out;
        std::vector<std::byte> err;
    };

    class Command {
    public:
        enum StdioType {
            NUL,
            INHERIT,
            PIPED
        };

        explicit Command(std::filesystem::path path);

    private:
        [[nodiscard]] tl::expected<ChildProcess, std::error_code> spawn(std::array<StdioType, 3> defaultTypes) const;

    public:
        Command &arg(std::string arg);
        Command &args(std::vector<std::string> args);
        Command &currentDirectory(std::filesystem::path path);
        Command &env(std::string key, std::string value);
        Command &envs(const std::map<std::string, std::string> &envs);
        Command &clearEnv();
        Command &removeEnv(const std::string &key);
        Command &stdIntput(StdioType type);
        Command &stdOutput(StdioType type);
        Command &stdError(StdioType type);

        [[nodiscard]] const std::filesystem::path &program() const;
        [[nodiscard]] const std::vector<std::string> &args() const;
        [[nodiscard]] const std::optional<std::filesystem::path> &currentDirectory() const;
        [[nodiscard]] const std::map<std::string, std::optional<std::string>> &envs() const;

        [[nodiscard]] tl::expected<ChildProcess, std::error_code> spawn() const;
        [[nodiscard]] tl::expected<Output, std::error_code> output() const;

    private:
        bool mInheritEnv;
        std::filesystem::path mPath;
        std::vector<std::string> mArguments;
        std::map<std::string, std::optional<std::string>> mEnviron;
        std::optional<std::filesystem::path> mCurrentDirectory;
        std::array<std::optional<StdioType>, 3> mStdioTypes;
    };
}

#endif //ZERO_PROCESS_H
