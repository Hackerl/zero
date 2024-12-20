#ifndef ZERO_PROCESS_H
#define ZERO_PROCESS_H

#include <fmt/core.h>

#ifdef _WIN32
#include "windows/process.h"
#elif defined(__APPLE__)
#include "macos/process.h"
#elif defined(__linux__)
#include "linux/process.h"
#endif

#if defined(_WIN32) || (defined(__ANDROID__) && __ANDROID_API__ < 23)
#include <zero/error.h>
#endif

namespace zero::os::process {
#ifdef _WIN32
    using ProcessImpl = windows::process::Process;
#elif defined(__APPLE__)
    using ProcessImpl = macos::process::Process;
#elif defined(__linux__)
    using ProcessImpl = linux::process::Process;
#endif
    using ID = std::uint32_t;

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
        [[nodiscard]] std::expected<ID, std::error_code> ppid() const;

        [[nodiscard]] std::expected<std::string, std::error_code> name() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] std::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] std::expected<std::map<std::string, std::string>, std::error_code> envs() const;
        [[nodiscard]] std::expected<std::chrono::system_clock::time_point, std::error_code> startTime() const;

        [[nodiscard]] std::expected<CPUTime, std::error_code> cpu() const;
        [[nodiscard]] std::expected<MemoryStat, std::error_code> memory() const;
        [[nodiscard]] std::expected<IOStat, std::error_code> io() const;

        std::expected<void, std::error_code> kill();

    private:
        ProcessImpl mImpl;
    };

    std::expected<Process, std::error_code> self();
    std::expected<Process, std::error_code> open(ID pid);
    std::expected<std::list<ID>, std::error_code> all();

    class ExitStatus {
    public:
#ifdef _WIN32
        using Status = DWORD;
#else
        using Status = int;
#endif

        explicit ExitStatus(Status status);

        [[nodiscard]] Status raw() const;
        [[nodiscard]] bool success() const;
        [[nodiscard]] std::optional<int> code() const;

#ifndef _WIN32
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

        std::expected<ExitStatus, std::error_code> wait();
        std::expected<std::optional<ExitStatus>, std::error_code> tryWait();

    private:
        std::array<std::optional<StdioFile>, 3> mStdio;
    };

    class Command;

#ifdef _WIN32
    class PseudoConsole {
    public:
        DEFINE_ERROR_CODE_INNER_EX(
            Error,
            "zero::os::process::PseudoConsole",
            API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported
        )

        PseudoConsole(HPCON pc, const std::array<HANDLE, 3> &handles);
        PseudoConsole(PseudoConsole &&rhs) noexcept;
        PseudoConsole &operator=(PseudoConsole &&rhs) noexcept;
        ~PseudoConsole();

        static std::expected<PseudoConsole, std::error_code> make(short rows, short columns);

        void close();
        std::expected<void, std::error_code> resize(short rows, short columns);
        std::expected<ChildProcess, std::error_code> spawn(const Command &command);

        HANDLE &file();

    private:
        HPCON mPC;
        std::array<HANDLE, 3> mHandles;
    };
#else
    class PseudoConsole {
    public:
#if defined(__ANDROID__) && __ANDROID_API__ < 23
        DEFINE_ERROR_CODE_INNER_EX(
            Error,
            "zero::os::process::PseudoConsole",
            API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported
        )
#endif
        PseudoConsole(int master, int slave);
        PseudoConsole(PseudoConsole &&rhs) noexcept;
        PseudoConsole &operator=(PseudoConsole &&rhs) noexcept;
        ~PseudoConsole();

        static std::expected<PseudoConsole, std::error_code> make(short rows, short columns);

        std::expected<void, std::error_code> resize(short rows, short columns);
        std::expected<ChildProcess, std::error_code> spawn(const Command &command);

        int &file();

    private:
        int mMaster;
        int mSlave;
    };
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

        [[nodiscard]] const std::filesystem::path &program() const;
        [[nodiscard]] const std::vector<std::string> &args() const;
        [[nodiscard]] const std::optional<std::filesystem::path> &currentDirectory() const;
        [[nodiscard]] const std::map<std::string, std::optional<std::string>> &envs() const;

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

        [[nodiscard]] std::expected<ChildProcess, std::error_code>
        spawn(const std::array<StdioType, 3> &defaultTypes) const;

        [[nodiscard]] std::expected<ChildProcess, std::error_code> spawn() const;
        [[nodiscard]] std::expected<ExitStatus, std::error_code> status() const;
        [[nodiscard]] std::expected<Output, std::error_code> output() const;

    private:
        bool mInheritEnv;
        std::filesystem::path mPath;
        std::vector<std::string> mArguments;
        std::map<std::string, std::optional<std::string>> mEnviron;
        std::optional<std::filesystem::path> mCurrentDirectory;
        std::array<std::optional<StdioType>, 3> mStdioTypes;

        friend class PseudoConsole;
    };
}

#if defined(_WIN32) || (defined(__ANDROID__) && __ANDROID_API__ < 23)
DECLARE_ERROR_CODE(zero::os::process::PseudoConsole::Error)
#endif

template<typename Char>
struct fmt::formatter<zero::os::process::ExitStatus, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    auto format(const zero::os::process::ExitStatus &status, FmtContext &ctx) const {
        if (const auto code = status.code())
            return fmt::format_to(ctx.out(), "exit code({})", *code);

#ifndef _WIN32
        if (const auto signal = status.signal()) {
            if (status.coreDumped())
                return fmt::format_to(ctx.out(), "core dumped({})", strsignal(*signal));

            return fmt::format_to(ctx.out(), "signal({})", strsignal(*signal));
        }

        if (const auto signal = status.stoppedSignal())
            return fmt::format_to(ctx.out(), "stopped({})", strsignal(*signal));

        using namespace std::string_view_literals;

        if (status.continued())
            return std::ranges::copy("continued(WIFCONTINUED)"sv, ctx.out()).out;
#endif

        return fmt::format_to(ctx.out(), "unrecognised wait status()", status.raw());
    }
};

#endif //ZERO_PROCESS_H
