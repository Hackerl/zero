#ifndef ZERO_OS_PROCESS_H
#define ZERO_OS_PROCESS_H

#include <cstdint>

namespace zero::os::process {
    using ID = std::uint32_t;

    ID currentProcessID();
}

#ifndef ZERO_PROCESS_PARTIAL_API
#include <variant>
#include <fmt/format.h>

#ifdef _WIN32
#include "windows/process.h"
#elifdef __APPLE__
#include "resource.h"
#include "macos/process.h"
#include <sys/types.h>
#elifdef __linux__
#include "linux/process.h"
#include <sys/types.h>
#endif

namespace zero::os::process {
#ifdef _WIN32
    using ProcessImpl = windows::process::Process;
#elifdef __APPLE__
    using ProcessImpl = macos::process::Process;
#elifdef __linux__
    using ProcessImpl = linux::process::Process;
#endif
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

        [[nodiscard]] std::expected<std::string, std::error_code> user() const;

        std::expected<void, std::error_code> kill();

    private:
        ProcessImpl mImpl;
    };

    Process self();
    std::expected<Process, std::error_code> open(ID pid);
    std::list<ID> all();

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

        ExitStatus wait();
        std::optional<ExitStatus> tryWait();

    private:
        std::array<std::optional<IOResource>, 3> mStdio;
    };

    class Command;

#ifdef _WIN32
    class PseudoConsole {
    public:
        using IOResource = IOResource;

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
        void resize(short rows, short columns);
        std::expected<ChildProcess, std::error_code> spawn(Command command);

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

        void resize(short rows, short columns);
        std::expected<ChildProcess, std::error_code> spawn(Command command);

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
        class Stdio {
            struct Null {
            };

            struct Inherit {
            };

            struct Piped {
            };

            using Core = std::variant<Null, Inherit, Piped, IOResource>;

            explicit Stdio(Core core);

        public:
            static Stdio null();
            static Stdio inherit();
            static Stdio piped();
            static Stdio from(IOResource resource);

        private:
            Core mCore;

            friend class Command;
        };

        explicit Command(std::filesystem::path path);

        [[nodiscard]] const std::filesystem::path &program() const;
        [[nodiscard]] const std::vector<std::string> &args() const;
        [[nodiscard]] const std::optional<std::filesystem::path> &currentDirectory() const;
        [[nodiscard]] const std::map<std::string, std::optional<std::string>> &envs() const;
        [[nodiscard]] const std::vector<Resource> &inheritedResources() const;

        template<meta::Mutable Self>
        Self &&arg(this Self &&self, std::string arg) {
            self.mArguments.push_back(std::move(arg));
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&args(this Self &&self, std::vector<std::string> args) {
            self.mArguments = std::move(args);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&currentDirectory(this Self &&self, std::filesystem::path path) {
            self.mCurrentDirectory = std::move(path);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&env(this Self &&self, std::string key, std::string value) {
            self.mEnviron[std::move(key)] = std::move(value);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&envs(this Self &&self, std::map<std::string, std::string> envs) {
            for (auto &[key, value]: envs)
                self.mEnviron[key] = std::move(value);

            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&clearEnv(this Self &&self) {
            self.mInheritEnv = false;
            self.mEnviron.clear();
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&removeEnv(this Self &&self, const std::string &key) {
            if (!self.mInheritEnv) {
                self.mEnviron.erase(key);
                return std::forward<Self>(self);
            }

            self.mEnviron[key] = std::nullopt;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&inheritedResource(this Self &&self, Resource resource) {
            self.mInheritedResources.push_back(std::move(resource));
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&inheritedResources(this Self &&self, std::vector<Resource> resources) {
            self.mInheritedResources = std::move(resources);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&stdInput(this Self &&self, Stdio stdio) {
            self.mStdioTypes[0] = std::move(stdio);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&stdOutput(this Self &&self, Stdio stdio) {
            self.mStdioTypes[1] = std::move(stdio);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&stdError(this Self &&self, Stdio stdio) {
            self.mStdioTypes[2] = std::move(stdio);
            return std::forward<Self>(self);
        }

#ifdef _WIN32
        template<meta::Mutable Self>
        Self &&creationFlags(this Self &&self, const DWORD flags) {
            self.mCreationFlags = flags;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&showWindow(this Self &&self, const WORD show) {
            self.mShowWindow = show;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&rawAttribute(this Self &&self, const DWORD_PTR attribute, const PVOID value, const SIZE_T size) {
            self.mRawAttributes.emplace_back(attribute, value, size);
            return std::forward<Self>(self);
        }
#else
        template<meta::Mutable Self>
        Self &&setSID(this Self &&self) {
            self.mSetSID = true;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&arg0(this Self &&self, std::string name) {
            self.mArg0 = std::move(name);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&processGroup(this Self &&self, const pid_t pgid) {
            self.mProcessGroup = pgid;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&uid(this Self &&self, const uid_t uid) {
            self.mUID = uid;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&gid(this Self &&self, const gid_t gid) {
            self.mGID = gid;
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&groups(this Self &&self, std::vector<gid_t> groups) {
            self.mGroups = std::move(groups);
            return std::forward<Self>(self);
        }

        template<meta::Mutable Self>
        Self &&preExec(this Self &&self, std::function<std::expected<void, std::error_code>()> f) {
            self.mPreExec = std::move(f);
            return std::forward<Self>(self);
        }
#endif

        [[nodiscard]] std::expected<ChildProcess, std::error_code>
        spawn(const std::array<Stdio, 3> &defaultStdio) const;

        [[nodiscard]] std::expected<ChildProcess, std::error_code> spawn() const;
        [[nodiscard]] std::expected<ExitStatus, std::error_code> status() const;
        [[nodiscard]] std::expected<Output, std::error_code> output() const;

    private:
        bool mInheritEnv;
        std::filesystem::path mPath;
        std::vector<std::string> mArguments;
        std::map<std::string, std::optional<std::string>> mEnviron;
        std::optional<std::filesystem::path> mCurrentDirectory;
        std::array<std::optional<Stdio>, 3> mStdioTypes;
        std::vector<Resource> mInheritedResources;
#ifdef _WIN32
        DWORD mCreationFlags{0};
        std::optional<WORD> mShowWindow;
        std::vector<std::tuple<DWORD_PTR, PVOID, SIZE_T>> mRawAttributes;
#else
        bool mSetSID{false};
        std::optional<std::string> mArg0;
        std::optional<pid_t> mProcessGroup;
        std::optional<uid_t> mUID;
        std::optional<gid_t> mGID;
        std::optional<std::vector<gid_t>> mGroups;
        std::function<std::expected<void, std::error_code>()> mPreExec;
#endif

        friend class PseudoConsole;
    };
}

template<typename Char>
struct fmt::formatter<zero::os::process::ExitStatus, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const zero::os::process::ExitStatus &status, FmtContext &ctx) {
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
#endif

#endif //ZERO_OS_PROCESS_H
