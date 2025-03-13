#ifndef ZERO_OS_PROCESS_H
#define ZERO_OS_PROCESS_H

#include <fmt/core.h>

#ifdef _WIN32
#include "windows/process.h"
#elif defined(__APPLE__)
#include "resource.h"
#include "macos/process.h"
#elif defined(__linux__)
#include "linux/process.h"
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
        using Native = DWORD;
#else
        using Native = int;
#endif

        explicit ExitStatus(Native status);

        [[nodiscard]] Native raw() const;
        [[nodiscard]] bool success() const;
        [[nodiscard]] std::optional<int> code() const;

#ifndef _WIN32
        [[nodiscard]] std::optional<int> signal() const;
        [[nodiscard]] std::optional<int> stoppedSignal() const;

        [[nodiscard]] bool coreDumped() const;
        [[nodiscard]] bool continued() const;
#endif

    private:
        Native mStatus;
    };

    class ChildProcess final : public Process {
    public:
        ChildProcess(Process process, std::array<std::optional<IOResource>, 3> stdio);

        std::optional<IOResource> &stdInput();
        std::optional<IOResource> &stdOutput();
        std::optional<IOResource> &stdError();

        std::expected<ExitStatus, std::error_code> wait();
        std::expected<std::optional<ExitStatus>, std::error_code> tryWait();

    private:
        std::array<std::optional<IOResource>, 3> mStdio;
    };

    class Command;

#ifdef _WIN32
    class PseudoConsole {
    public:
        using IOResource = IOResource;

        DEFINE_ERROR_CODE_INNER_EX(
            Error,
            "zero::os::process::PseudoConsole",
            API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported
        )

        struct Endpoint {
            IOResource reader;
            IOResource writer;
        };

        PseudoConsole(HPCON pc, Endpoint master, Endpoint slave);
        PseudoConsole(PseudoConsole &&rhs) noexcept;
        PseudoConsole &operator=(PseudoConsole &&rhs) noexcept;
        ~PseudoConsole();

        static std::expected<PseudoConsole, std::error_code> make(short rows, short columns);

        void close();
        std::expected<void, std::error_code> resize(short rows, short columns);
        std::expected<ChildProcess, std::error_code> spawn(const Command &command);

        Endpoint &master();

    private:
        HPCON mPC;
        Endpoint mMaster;
        Endpoint mSlave;
    };
#else
    class PseudoConsole {
    public:
        class IOResource final : public os::IOResource {
        public:
            using os::IOResource::IOResource;
            std::expected<std::size_t, std::error_code> read(std::span<std::byte> data) override;
        };

        PseudoConsole(IOResource master, IOResource slave);
        static std::expected<PseudoConsole, std::error_code> make(short rows, short columns);

        std::expected<void, std::error_code> resize(short rows, short columns);
        std::expected<ChildProcess, std::error_code> spawn(const Command &command);

        IOResource &master();

    private:
        IOResource mMaster;
        IOResource mSlave;
    };
#endif

    struct Output {
        ExitStatus status;
        std::vector<std::byte> out;
        std::vector<std::byte> err;
    };

    class Command {
    public:
#if defined(__ANDROID__) && __ANDROID_API__ < 34
        DEFINE_ERROR_CODE_INNER_EX(
            Error,
            "zero::os::process::Command",
            API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported
        )
#endif
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
        [[nodiscard]] const std::vector<Resource> &inheritedResources() const;

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&arg(this Self &&self, std::string arg) {
            self.mArguments.push_back(std::move(arg));
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&args(this Self &&self, std::vector<std::string> args) {
            self.mArguments = std::move(args);
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&currentDirectory(this Self &&self, std::filesystem::path path) {
            self.mCurrentDirectory = std::move(path);
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&env(this Self &&self, std::string key, std::string value) {
            self.mEnviron[std::move(key)] = std::move(value);
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&envs(this Self &&self, std::map<std::string, std::string> envs) {
            for (auto &[key, value]: envs)
                self.mEnviron[key] = std::move(value);

            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&clearEnv(this Self &&self) {
            self.mInheritEnv = false;
            self.mEnviron.clear();
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&removeEnv(this Self &&self, const std::string &key) {
            if (!self.mInheritEnv) {
                self.mEnviron.erase(key);
                return std::forward<Self>(self);
            }

            self.mEnviron[key] = std::nullopt;
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&inheritedResource(this Self &&self, Resource resource) {
            self.mInheritedResources.push_back(std::move(resource));
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&inheritedResources(this Self &&self, std::vector<Resource> resource) {
            self.mInheritedResources = std::move(resource);
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&stdInput(this Self &&self, const StdioType type) {
            self.mStdioTypes[0] = type;
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&stdOutput(this Self &&self, const StdioType type) {
            self.mStdioTypes[1] = type;
            return std::forward<Self>(self);
        }

        template<typename Self>
            requires (!std::is_const_v<Self>)
        Self &&stdError(this Self &&self, const StdioType type) {
            self.mStdioTypes[2] = type;
            return std::forward<Self>(self);
        }

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
        std::vector<Resource> mInheritedResources;

        friend class PseudoConsole;
    };
}

#if defined(_WIN32)
DECLARE_ERROR_CODE(zero::os::process::PseudoConsole::Error)
#endif

#if defined(__ANDROID__) && __ANDROID_API__ < 34
DECLARE_ERROR_CODE(zero::os::process::Command::Error)
#endif

template<typename Char>
struct fmt::formatter<zero::os::process::ExitStatus, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    auto format(const zero::os::process::ExitStatus &status, FmtContext &ctx) const {
#ifdef _WIN32
        if (const auto raw = status.raw(); raw & 0x80000000)
            return fmt::format_to(ctx.out(), "exit code({:#x})", raw);
        else
            return fmt::format_to(ctx.out(), "exit code({})", raw);
#else
        if (const auto code = status.code())
            return fmt::format_to(ctx.out(), "exit code({})", *code);

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

        return fmt::format_to(ctx.out(), "unrecognised wait status()", status.raw());
#endif
    }
};

#endif //ZERO_OS_PROCESS_H
