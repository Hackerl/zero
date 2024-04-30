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
    using ProcessImpl = nt::process::Process;
#elif __APPLE__
    using ProcessImpl = darwin::process::Process;
#elif __linux__
    using ProcessImpl = procfs::process::Process;
#endif
    using ID = int;

    struct CPUTime {
        double user;
        double system;
    };

    struct MemoryStat {
        std::uint64_t rss;
        std::uint64_t vms;
    };

    struct IOStat {
        std::uint64_t readBytes;
        std::uint64_t writeBytes;
    };

    class Process {
    public:
        explicit Process(ProcessImpl impl);

        ProcessImpl &impl();
        [[nodiscard]] const ProcessImpl &impl() const;

        [[nodiscard]] ID pid() const;
        [[nodiscard]] tl::expected<ID, std::error_code> ppid() const;

        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> envs() const;

        [[nodiscard]] tl::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] tl::expected<IOStat, std::error_code> io() const;

        tl::expected<void, std::error_code> kill();

    private:
        ProcessImpl mImpl;
    };

    tl::expected<Process, std::error_code> self();
    tl::expected<Process, std::error_code> open(ID pid);
    tl::expected<std::list<ID>, std::error_code> all();

    class ExitStatus {
#ifdef _WIN32
        using Status = DWORD;
#else
        using Status = int;
#endif

    public:
        explicit ExitStatus(Status status);

        [[nodiscard]] bool success() const;
        [[nodiscard]] std::optional<int> code() const;

#ifdef __unix__
        [[nodiscard]] std::optional<int> signal() const;
        [[nodiscard]] std::optional<int> stoppedSignal() const;

        [[nodiscard]] bool coreDumped() const;
        [[nodiscard]] bool continued() const;
#endif

    private:
        Status mStatus;
    };

    class ChildProcess final : public Process {
    public:
#ifdef _WIN32
        using StdioFile = HANDLE;
#else
        using StdioFile = int;
#endif
        ChildProcess(Process process, const std::array<std::optional<StdioFile>, 3> &stdio);
        ChildProcess(ChildProcess &&rhs) noexcept;
        ChildProcess &operator=(ChildProcess &&rhs) noexcept;
        ~ChildProcess();

        std::optional<StdioFile> &stdInput();
        std::optional<StdioFile> &stdOutput();
        std::optional<StdioFile> &stdError();

        tl::expected<ExitStatus, std::error_code> wait();
        tl::expected<std::optional<ExitStatus>, std::error_code> tryWait();

    private:
        std::array<std::optional<StdioFile>, 3> mStdio;
    };

#ifdef _WIN32
    class PseudoConsole {
    public:
        enum class Error {
            API_NOT_AVAILABLE = 1,
        };

        class ErrorCategory final : public std::error_category {
        public:
            [[nodiscard]] const char *name() const noexcept override;
            [[nodiscard]] std::string message(int value) const override;
            [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
        };

        PseudoConsole(HPCON pc, const std::array<HANDLE, 4> &handles);
        PseudoConsole(PseudoConsole &&rhs) noexcept;
        PseudoConsole &operator=(PseudoConsole &&rhs) noexcept;
        ~PseudoConsole();

        static tl::expected<PseudoConsole, std::error_code> make(short rows, short columns);

        void close();
        tl::expected<void, std::error_code> resize(short rows, short columns);

        HANDLE &input();
        HANDLE &output();

    private:
        HPCON mPC;
        std::array<HANDLE, 4> mHandles;

        friend class Command;
    };
#else
    class PseudoConsole {
    public:
#if __ANDROID__ && __ANDROID_API__ < 23
        enum class Error {
            API_NOT_AVAILABLE = 1,
        };

        class ErrorCategory final : public std::error_category {
        public:
            [[nodiscard]] const char *name() const noexcept override;
            [[nodiscard]] std::string message(int value) const override;
            [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override;
        };
#endif
        PseudoConsole(int master, int slave);
        PseudoConsole(PseudoConsole &&rhs) noexcept;
        PseudoConsole &operator=(PseudoConsole &&rhs) noexcept;
        ~PseudoConsole();

        static tl::expected<PseudoConsole, std::error_code> make(short rows, short columns);

        tl::expected<void, std::error_code> resize(short rows, short columns);
        int &fd();

    private:
        int mMaster;
        int mSlave;

        friend class Command;
    };
#endif

#if _WIN32 || (__ANDROID__ && __ANDROID_API__ < 23)
    std::error_code make_error_code(PseudoConsole::Error e);
#endif

    struct Output {
        ExitStatus status;
        std::vector<std::byte> out;
        std::vector<std::byte> err;
    };

    class Command {
    public:
        enum class StdioType {
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
        Command &envs(std::map<std::string, std::string> envs);
        Command &clearEnv();
        Command &removeEnv(const std::string &key);
        Command &stdInput(StdioType type);
        Command &stdOutput(StdioType type);
        Command &stdError(StdioType type);

        [[nodiscard]] const std::filesystem::path &program() const;
        [[nodiscard]] const std::vector<std::string> &args() const;
        [[nodiscard]] const std::optional<std::filesystem::path> &currentDirectory() const;
        [[nodiscard]] const std::map<std::string, std::optional<std::string>> &envs() const;

        [[nodiscard]] tl::expected<ChildProcess, std::error_code> spawn() const;
        [[nodiscard]] tl::expected<ChildProcess, std::error_code> spawn(PseudoConsole &pc) const;
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

#if _WIN32 || (__ANDROID__ && __ANDROID_API__ < 23)
template<>
struct std::is_error_code_enum<zero::os::process::PseudoConsole::Error> : std::true_type {
};
#endif

#endif //ZERO_PROCESS_H
