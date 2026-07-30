#include <zero/os/process.h>

#ifndef _WIN32
#include <unistd.h>
#endif

zero::os::process::ID zero::os::process::currentProcessID() {
#ifdef _WIN32
    return static_cast<ID>(GetCurrentProcessId());
#else
    return getpid();
#endif
}

#ifndef ZERO_PROCESS_PARTIAL_API
#include <zero/os/os.h>
#include <zero/expect.h>
#include <zero/defer.h>
#include <zero/env.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <cassert>
#include <ranges>
#include <future>

#ifdef _WIN32
#include <zero/filesystem.h>
#include <zero/strings.h>
#include <zero/os/windows/error.h>
#else
#include <csignal>
#include <fcntl.h>
#include <grp.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <zero/os/unix/error.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#if (defined(__ANDROID__) && __ANDROID_API__ < 34) || defined(__OHOS__)
#include <dlfcn.h>
#endif
#endif

#ifdef __APPLE__
extern char **environ;
#endif

constexpr auto StdinR = 0;
constexpr auto StdinW = 1;
constexpr auto StdoutR = 2;
constexpr auto StdoutW = 3;
constexpr auto StderrR = 4;
constexpr auto StderrW = 5;

#ifndef _WIN32
constexpr auto FDScanLimit = 65535;
#endif

#ifdef _WIN32
namespace {
    const auto createPseudoConsole = reinterpret_cast<decltype(&CreatePseudoConsole)>(
        GetProcAddress(GetModuleHandleA("kernel32"), "CreatePseudoConsole")
    );

    const auto closePseudoConsole = reinterpret_cast<decltype(&ClosePseudoConsole)>(
        GetProcAddress(GetModuleHandleA("kernel32"), "ClosePseudoConsole")
    );

    const auto resizePseudoConsole = reinterpret_cast<decltype(&ResizePseudoConsole)>(
        GetProcAddress(GetModuleHandleA("kernel32"), "ResizePseudoConsole")
    );

    std::string quote(const std::string &arg) {
        std::string result;

        const auto str = arg.c_str();
        const auto quote = std::strchr(str, ' ') || std::strchr(str, '\t') || *str == '\0';

        if (quote)
            result.push_back('\"');

        int bsCount{0};

        for (auto p = str; *p != '\0'; ++p) {
            if (*p == '\\') {
                ++bsCount;
            }
            else if (*p == '\"') {
                result.append(bsCount * 2 + 1, '\\');
                result.push_back('\"');
                bsCount = 0;
            }
            else {
                result.append(bsCount, '\\');
                bsCount = 0;
                result.push_back(*p);
            }
        }

        if (quote) {
            result.append(bsCount * 2, '\\');
            result.push_back('\"');
        }
        else {
            result.append(bsCount, '\\');
        }

        return result;
    }
}
#endif

zero::os::process::Process::Process(ProcessImpl impl) : mImpl{std::move(impl)} {
}

zero::os::process::ProcessImpl &zero::os::process::Process::impl() {
    return mImpl;
}

const zero::os::process::ProcessImpl &zero::os::process::Process::impl() const {
    return mImpl;
}

zero::os::process::ID zero::os::process::Process::pid() const {
    return static_cast<ID>(mImpl.pid());
}

std::expected<zero::os::process::ID, std::error_code> zero::os::process::Process::ppid() const {
    return mImpl.ppid().transform([](const auto &id) {
        return static_cast<ID>(id);
    });
}

std::expected<std::string, std::error_code> zero::os::process::Process::name() const {
#ifdef _WIN32
    return mImpl.name();
#else
    return mImpl.comm();
#endif
}

std::expected<std::filesystem::path, std::error_code> zero::os::process::Process::cwd() const {
    return mImpl.cwd();
}

std::expected<std::filesystem::path, std::error_code> zero::os::process::Process::exe() const {
    return mImpl.exe();
}

std::expected<std::vector<std::string>, std::error_code> zero::os::process::Process::cmdline() const {
    return mImpl.cmdline();
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::process::Process::envs() const {
    return mImpl.envs();
}

std::expected<std::chrono::system_clock::time_point, std::error_code> zero::os::process::Process::startTime() const {
    return mImpl.startTime();
}

std::expected<zero::os::process::CPUTime, std::error_code> zero::os::process::Process::cpu() const {
    return mImpl.cpu().transform([](const auto &cpu) {
        return CPUTime{.user = cpu.user, .system = cpu.system};
    });
}

std::expected<zero::os::process::MemoryStat, std::error_code> zero::os::process::Process::memory() const {
    return mImpl.memory().transform([](const auto &memory) {
        return MemoryStat{.rss = memory.rss, .vms = memory.vms};
    });
}

std::expected<zero::os::process::IOStat, std::error_code> zero::os::process::Process::io() const {
    return mImpl.io().transform([](const auto &io) {
        return IOStat{.readBytes = io.readBytes, .writeBytes = io.writeBytes};
    });
}

std::expected<std::string, std::error_code> zero::os::process::Process::user() const {
    return mImpl.user();
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::process::Process::kill() {
#ifdef _WIN32
    return mImpl.terminate(EXIT_FAILURE);
#else
    return mImpl.kill(SIGKILL);
#endif
}

zero::os::process::Process zero::os::process::self() {
#ifdef _WIN32
    return Process{windows::process::self()};
#elifdef __APPLE__
    return Process{macos::process::self()};
#elifdef __linux__
    return Process{linux::process::self()};
#endif
}

std::expected<zero::os::process::Process, std::error_code> zero::os::process::open(const ID pid) {
#ifdef _WIN32
    return windows::process::open(pid)
#elifdef __APPLE__
    return macos::process::open(static_cast<pid_t>(pid))
#elifdef __linux__
    return linux::process::open(static_cast<pid_t>(pid))
#endif
        .transform([](ProcessImpl &&process) {
            return Process{std::move(process)};
        });
}

std::list<zero::os::process::ID> zero::os::process::all() {
#ifdef _WIN32
    return windows::process::all()
#elifdef __APPLE__
    return macos::process::all()
#elifdef __linux__
    return linux::process::all()
#endif
        | std::views::transform([](const auto &pid) {
            return static_cast<ID>(pid);
        })
        | std::ranges::to<std::list>();
}

zero::os::process::ExitStatus::ExitStatus(const Native status) : mStatus{status} {
}

zero::os::process::ExitStatus::Native zero::os::process::ExitStatus::raw() const {
    return mStatus;
}

bool zero::os::process::ExitStatus::success() const {
#ifdef _WIN32
    return mStatus == EXIT_SUCCESS;
#else
    return WIFEXITED(mStatus) && WEXITSTATUS(mStatus) == 0;
#endif
}

std::optional<int> zero::os::process::ExitStatus::code() const {
#ifdef _WIN32
    return static_cast<int>(mStatus);
#else
    if (!(WIFEXITED(mStatus)))
        return std::nullopt;

    return WEXITSTATUS(mStatus);
#endif
}

#ifndef _WIN32
std::optional<int> zero::os::process::ExitStatus::signal() const {
    if (!(WIFSIGNALED(mStatus)))
        return std::nullopt;

    return WTERMSIG(mStatus);
}

std::optional<int> zero::os::process::ExitStatus::stoppedSignal() const {
    if (!(WIFSTOPPED(mStatus)))
        return std::nullopt;

    return WSTOPSIG(mStatus);
}

bool zero::os::process::ExitStatus::coreDumped() const {
    return WIFSIGNALED(mStatus) && WCOREDUMP(mStatus);
}

bool zero::os::process::ExitStatus::continued() const {
    return WIFCONTINUED(mStatus);
}
#endif

zero::os::process::ChildProcess::ChildProcess(Process process, std::array<std::optional<IOResource>, 3> stdio)
    : Process{std::move(process)}, mStdio{std::move(stdio)} {
}

std::optional<zero::os::IOResource> &zero::os::process::ChildProcess::stdInput() {
    return mStdio[0];
}

std::optional<zero::os::IOResource> &zero::os::process::ChildProcess::stdOutput() {
    return mStdio[1];
}

std::optional<zero::os::IOResource> &zero::os::process::ChildProcess::stdError() {
    return mStdio[2];
}

#ifdef _WIN32
zero::os::process::PseudoConsole::PseudoConsole(const HPCON pc, Endpoint master, Endpoint slave)
    : mPC{pc}, mMaster{std::move(master)}, mSlave{std::move(slave)} {
}

zero::os::process::PseudoConsole::PseudoConsole(PseudoConsole &&rhs) noexcept
    : mPC{std::exchange(rhs.mPC, nullptr)}, mMaster{std::move(rhs.mMaster)}, mSlave{std::move(rhs.mSlave)} {
}

zero::os::process::PseudoConsole &zero::os::process::PseudoConsole::operator=(PseudoConsole &&rhs) noexcept {
    mPC = std::exchange(rhs.mPC, nullptr);
    mMaster = std::move(rhs.mMaster);
    mSlave = std::move(rhs.mSlave);
    return *this;
}

zero::os::process::PseudoConsole::~PseudoConsole() {
    if (!mPC)
        return;

    closePseudoConsole(mPC);
}

std::expected<zero::os::process::PseudoConsole, std::error_code>
zero::os::process::PseudoConsole::make(const short rows, const short columns) {
    if (!createPseudoConsole || !closePseudoConsole || !resizePseudoConsole)
        throw error::StacktraceError<std::runtime_error>{"Pseudo console API is not supported on this system"};

    auto [inReader, inWriter] = pipe();
    auto [outReader, outWriter] = pipe();

    HPCON hPC{};

    if (const auto result = createPseudoConsole(
        {.X = columns, .Y = rows},
        inReader.fd(),
        outWriter.fd(),
        0,
        &hPC
    ); result != S_OK)
        return std::unexpected{static_cast<windows::ResultHandle>(result)};

    return PseudoConsole{
        hPC,
        Endpoint{.reader = std::move(outReader), .writer = std::move(inWriter)},
        Endpoint{.reader = std::move(inReader), .writer = std::move(outWriter)}
    };
}

void zero::os::process::PseudoConsole::close() {
    assert(mPC);
    closePseudoConsole(std::exchange(mPC, nullptr));
}

// ReSharper disable once CppMemberFunctionMayBeConst
void zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    if (const auto result = resizePseudoConsole(mPC, {.X = columns, .Y = rows}); result != S_OK)
        throw error::StacktraceError<std::system_error>{static_cast<windows::ResultHandle>(result)};
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::PseudoConsole::spawn(Command command) {
    assert(mSlave.reader);
    assert(mSlave.writer);

    auto child = command
                 .stdInput(Command::Stdio::from(IOResource{nullptr}))
                 .stdOutput(Command::Stdio::from(IOResource{nullptr}))
                 .stdError(Command::Stdio::from(IOResource{nullptr}))
                 .rawAttribute(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, mPC, sizeof(mPC))
                 .spawn();
    Z_EXPECT(child);

    error::guard(mSlave.reader.close());
    error::guard(mSlave.writer.close());

    return child;
}

zero::os::process::PseudoConsole::Endpoint &zero::os::process::PseudoConsole::master() {
    return mMaster;
}
#else
std::expected<std::size_t, std::error_code>
zero::os::process::PseudoConsole::IOResource::read(const std::span<std::byte> data) {
    return os::IOResource::read(data)
        .or_else([](const auto &ec) -> std::expected<std::size_t, std::error_code> {
            if (ec != std::errc::io_error)
                return std::unexpected{ec};

            return 0;
        });
}

zero::os::process::PseudoConsole::PseudoConsole(IOResource master, IOResource slave)
    : mMaster{std::move(master)}, mSlave{std::move(slave)} {
}

std::expected<zero::os::process::PseudoConsole, std::error_code>
zero::os::process::PseudoConsole::make(const short rows, const short columns) {
    int master{}, slave{};

    Z_EXPECT(unix::expected([&] {
        return openpty(&master, &slave, nullptr, nullptr, nullptr);
    }));

    PseudoConsole pc{IOResource{master}, IOResource{slave}};
    pc.resize(rows, columns);

    return pc;
}

// ReSharper disable once CppMemberFunctionMayBeConst
void zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    winsize ws{};

    ws.ws_row = rows;
    ws.ws_col = columns;

    error::guard(unix::expected([&] {
        return ioctl(mMaster.fd(), TIOCSWINSZ, &ws);
    }));
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::PseudoConsole::spawn(Command command) {
    assert(mSlave.fd() > STDERR_FILENO);

    auto child = command
                 .setSID()
                 .preExec([]() -> std::expected<void, std::error_code> {
                     Z_EXPECT(unix::expected([] {
                         return ioctl(STDIN_FILENO, TIOCSCTTY, nullptr);
                     }));
                     return {};
                 })
                 .stdInput(Command::Stdio::from(mSlave.duplicate(true)))
                 .stdOutput(Command::Stdio::from(mSlave.duplicate(true)))
                 .stdError(Command::Stdio::from(mSlave.duplicate(true)))
                 .spawn();
    Z_EXPECT(child);

    error::guard(mSlave.close());
    return child;
}

zero::os::process::PseudoConsole::IOResource &zero::os::process::PseudoConsole::master() {
    return mMaster;
}
#endif

// ReSharper disable once CppMemberFunctionMayBeConst
zero::os::process::ExitStatus zero::os::process::ChildProcess::wait() {
#ifdef _WIN32
    const auto &impl = this->impl();
    error::guard(impl.wait(std::nullopt));
    return ExitStatus{error::guard(impl.exitCode())};
#else
    int s{};
    const auto pid = this->impl().pid();

    const auto id = error::guard(unix::ensure([&] {
        return waitpid(pid, &s, 0);
    }));
    assert(id == pid);

    return ExitStatus{s};
#endif
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::optional<zero::os::process::ExitStatus>
zero::os::process::ChildProcess::tryWait() {
#ifdef _WIN32
    using namespace std::chrono_literals;

    const auto &impl = this->impl();

    if (const auto result = impl.wait(0ms); !result) {
        if (const auto &error = result.error(); error != std::errc::timed_out)
            throw error::StacktraceError<std::system_error>{error};

        return std::nullopt;
    }

    return ExitStatus{error::guard(impl.exitCode())};
#else
    int s{};
    const auto pid = this->impl().pid();

    const auto id = error::guard(unix::expected([&] {
        return waitpid(pid, &s, WNOHANG);
    }));

    if (id == 0)
        return std::nullopt;

    return ExitStatus{s};
#endif
}

zero::os::process::Command::Stdio::Stdio(Core core) : mCore{std::move(core)} {
}

zero::os::process::Command::Stdio zero::os::process::Command::Stdio::null() {
    return Stdio{Null{}};
}

zero::os::process::Command::Stdio zero::os::process::Command::Stdio::inherit() {
    return Stdio{Inherit{}};
}

zero::os::process::Command::Stdio zero::os::process::Command::Stdio::piped() {
    return Stdio{Piped{}};
}

zero::os::process::Command::Stdio zero::os::process::Command::Stdio::from(IOResource resource) {
    return Stdio{std::move(resource)};
}

zero::os::process::Command::Command(std::filesystem::path path) : mInheritEnv{true}, mPath{std::move(path)} {
}

const std::filesystem::path &zero::os::process::Command::program() const {
    return mPath;
}

const std::vector<std::string> &zero::os::process::Command::args() const {
    return mArguments;
}

const std::optional<std::filesystem::path> &zero::os::process::Command::currentDirectory() const {
    return mCurrentDirectory;
}

const std::map<std::string, std::optional<std::string>> &zero::os::process::Command::envs() const {
    return mEnviron;
}

const std::vector<zero::os::Resource> &zero::os::process::Command::inheritedResources() const {
    return mInheritedResources;
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::Command::spawn(const std::array<Stdio, 3> &defaultStdio) const {
    assert(
        std::ranges::all_of(
            mInheritedResources,
            [](const auto &resource) {
                return resource.isInheritable();
            }
        )
    );
#ifdef _WIN32
    std::array<std::optional<IOResource>, 6> resources;

    constexpr std::array indexMapping{StdinR, StdoutW, StderrW};
    constexpr std::array typeMapping{STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    constexpr std::array<DWORD, 3> accessMapping{GENERIC_READ, GENERIC_WRITE, GENERIC_WRITE};

    for (int i{0}; i < 3; ++i) {
        const auto &stdio = mStdioTypes[i] ? *mStdioTypes[i] : defaultStdio[i];

        if (std::holds_alternative<Stdio::Inherit>(stdio.mCore)) {
            const auto handle = GetStdHandle(typeMapping[i]);

            if (handle == INVALID_HANDLE_VALUE)
                throw error::StacktraceError<std::system_error>{
                    static_cast<int>(GetLastError()), std::system_category()
                };

            if (!handle)
                continue;

            IOResource resource{handle};
            Z_DEFER(std::ignore = resource.release());

            resources[indexMapping[i]] = resource.duplicate(true);
            continue;
        }

        if (std::holds_alternative<Stdio::Piped>(stdio.mCore)) {
            auto [reader, writer] = pipe();

            reader.setInheritable(true);
            writer.setInheritable(true);

            resources[i * 2] = std::move(reader);
            resources[i * 2 + 1] = std::move(writer);
            continue;
        }

        if (const auto *resource = std::get_if<IOResource>(&stdio.mCore)) {
            // NULL stdio handles are allowed when STARTF_USESTDHANDLES is set. This also covers
            // the PseudoConsole case, which requires the flag but does not need stdio handles.
            if (!*resource)
                continue;

            resources[indexMapping[i]] = resource->duplicate(true);
            continue;
        }

        SECURITY_ATTRIBUTES saAttr{};

        saAttr.nLength = sizeof(saAttr);
        saAttr.bInheritHandle = true;
        saAttr.lpSecurityDescriptor = nullptr;

        const auto handle = CreateFileA(
            R"(\\.\NUL)",
            accessMapping[i],
            0,
            &saAttr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE)
            throw error::StacktraceError<std::system_error>{static_cast<int>(GetLastError()), std::system_category()};

        resources[indexMapping[i]].emplace(handle);
    }

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(siEx);

    constexpr std::array memberPointers{
        &STARTUPINFOW::hStdInput,
        &STARTUPINFOW::hStdOutput,
        &STARTUPINFOW::hStdError
    };

    auto inheritedHandles = mInheritedResources
        | std::views::transform(&Resource::get)
        | std::ranges::to<std::vector>();

    for (int i{0}; i < 3; ++i) {
        const auto &resource = resources[indexMapping[i]];

        if (!resource)
            continue;

        const auto fd = resource->fd();
        siEx.StartupInfo.*memberPointers[i] = fd;
        inheritedHandles.push_back(fd);
    }

    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    if (mShowWindow) {
        siEx.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
        siEx.StartupInfo.wShowWindow = *mShowWindow;
    }

    const auto attributeCount = static_cast<DWORD>(1 + mRawAttributes.size());

    SIZE_T size{};
    InitializeProcThreadAttributeList(nullptr, attributeCount, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    error::guard(windows::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, attributeCount, 0, &size);
    }));

    if (!inheritedHandles.empty())
        error::guard(windows::expected([&] {
            return UpdateProcThreadAttribute(
                siEx.lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                inheritedHandles.data(),
                inheritedHandles.size() * sizeof(HANDLE),
                nullptr,
                nullptr
            );
        }));

    for (const auto &[attribute, value, attrSize]: mRawAttributes) {
        error::guard(windows::expected([&] {
            return UpdateProcThreadAttribute(
                siEx.lpAttributeList,
                0,
                attribute,
                value,
                attrSize,
                nullptr,
                nullptr
            );
        }));
    }

    std::map<std::string, std::string> envs;

    if (mInheritEnv)
        envs = env::list();

    for (const auto &[key, value]: mEnviron) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = error::guard(
        strings::decode(
            to_string(
                fmt::join(
                    envs | std::views::transform([](const auto &pair) {
                        auto env = pair.first + "=" + pair.second;
                        env.push_back('\0');
                        return env;
                    }),
                    ""
                )
            )
        )
    );

    std::vector arguments{filesystem::stringify(mPath)};
    arguments.append_range(mArguments);

    auto cmd = error::guard(
        strings::decode(
            to_string(
                fmt::join(arguments | std::views::transform(quote), " ")
            )
        )
    );

    PROCESS_INFORMATION info{};

    Z_EXPECT(windows::expected([&] {
        return CreateProcessW(
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            !inheritedHandles.empty(),
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | mCreationFlags,
            environment.data(),
            mCurrentDirectory ? mCurrentDirectory->wstring().c_str() : nullptr,
            &siEx.StartupInfo,
            &info
        );
    }));

    error::guard(windows::expected([&] {
        return CloseHandle(info.hThread);
    }));

    std::array<std::optional<IOResource>, 3> stdio;

    if (auto &resource = resources[StdinW])
        stdio[0] = *std::move(resource);

    if (auto &resource = resources[StdoutR])
        stdio[1] = *std::move(resource);

    if (auto &resource = resources[StderrR])
        stdio[2] = *std::move(resource);

    return ChildProcess{
        Process{windows::process::Process{Resource{info.hProcess}, info.dwProcessId}},
        std::move(stdio)
    };
#else
    std::array<std::optional<IOResource>, 6> resources{};

    constexpr std::array indexMapping{StdinR, StdoutW, StderrW};
    constexpr std::array flagMapping{O_RDONLY, O_WRONLY, O_WRONLY};

    for (int i{0}; i < 3; ++i) {
        const auto &stdio = mStdioTypes[i] ? *mStdioTypes[i] : defaultStdio[i];

        if (std::holds_alternative<Stdio::Inherit>(stdio.mCore))
            continue;

        if (std::holds_alternative<Stdio::Piped>(stdio.mCore)) {
            auto [reader, writer] = pipe();

            resources[i * 2] = std::move(reader);
            resources[i * 2 + 1] = std::move(writer);
            continue;
        }

        if (const auto *resource = std::get_if<IOResource>(&stdio.mCore)) {
            resources[indexMapping[i]] = resource->duplicate(true);
            continue;
        }

        resources[indexMapping[i]].emplace(
            error::guard(
                unix::expected([&] {
                    return ::open("/dev/null", flagMapping[i]);
                })
            )
        );
    }

    assert(
        std::ranges::all_of(
            resources,
            [](const auto &resource) {
                return !resource || resource->fd() > STDERR_FILENO;
            }
        )
    );

    const auto &program = mPath.native();

    std::vector arguments{mArg0.value_or(program)};
    arguments.append_range(mArguments);

    std::map<std::string, std::string> envs;

    if (mInheritEnv)
        envs = env::list();

    for (const auto &[key, value]: mEnviron) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = envs
        | std::views::transform([](const auto &pair) {
            return fmt::format("{}={}", pair.first, pair.second);
        })
        | std::ranges::to<std::vector>();

    const auto argv = std::make_unique<char *[]>(arguments.size() + 1);
    const auto envp = std::make_unique<char *[]>(environment.size() + 1);

    std::ranges::transform(
        arguments,
        argv.get(),
        [](auto &str) {
            return str.data();
        }
    );

    std::ranges::transform(
        environment,
        envp.get(),
        [](auto &str) {
            return str.data();
        }
    );

    errno = 0;
    auto fdLimit = sysconf(_SC_OPEN_MAX);

    if (fdLimit == -1) {
        if (errno != 0)
            throw error::StacktraceError<std::system_error>{errno, std::system_category()};

        fdLimit = FDScanLimit;
    }

    fdLimit = std::min<long>(fdLimit, FDScanLimit);

    pid_t pid{};

    if (mSetSID || mGroups || mPreExec || mUID || mGID) {
        auto [reader, writer] = pipe();
        writer.setInheritable(false);

        pid = error::guard(unix::expected(fork));

        if (pid == 0) {
            const auto writerFD = writer.fd();

            const auto guard = [&]<typename T>(std::expected<T, std::error_code> &&result) {
                if (!result) {
                    assert(result.error().category() == std::system_category());
                    const auto error = result.error().value();
                    const auto n = unix::ensure([&] {
                        return write(writerFD, &error, sizeof(error));
                    });
                    assert(n);
                    assert(*n == sizeof(error));
                    std::abort();
                }

                if constexpr (!std::is_void_v<T>)
                    return *std::move(result);
            };

            guard(reader.close());

            struct sigaction sa{};

            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            for (int n{1}; n < NSIG; ++n) {
                if (n == SIGKILL || n == SIGSTOP)
                    continue;

                std::ignore = unix::expected([&] {
                    return sigaction(n, &sa, nullptr);
                });
            }

            if (mSetSID)
                guard(unix::expected([&] {
                    return setsid();
                }));

            if (mProcessGroup)
                guard(unix::expected([&] {
                    return setpgid(0, *mProcessGroup);
                }));

            if (mGroups)
                guard(unix::expected([&] {
                    return setgroups(static_cast<int>(mGroups->size()), mGroups->data());
                }));

            if (mGID)
                guard(unix::expected([&] {
                    return setgid(*mGID);
                }));

            if (mUID)
                guard(unix::expected([&] {
                    return setuid(*mUID);
                }));

            for (int i{0}; i < 3; ++i) {
                if (const auto &resource = resources[indexMapping[i]]) {
                    guard(unix::ensure([&] {
                        return dup2(resource->fd(), i);
                    }));
                }
            }

            for (int fd{STDERR_FILENO + 1}; fd < fdLimit; ++fd) {
                if (fd == writerFD)
                    continue;

                if (std::ranges::find_if(
                    mInheritedResources,
                    [=](const auto &resource) {
                        return *resource == fd;
                    }
                ) != mInheritedResources.end())
                    continue;

                std::ignore = unix::expected([&] {
                    return close(fd);
                });
            }

            if (const auto &directory = mCurrentDirectory)
                guard(unix::expected([&] {
                    return chdir(directory->string().c_str());
                }));

            sigset_t set{};
            sigemptyset(&set);

            guard(unix::expected([&] {
                return sigprocmask(SIG_SETMASK, &set, nullptr);
            }));

            if (mPreExec)
                guard(mPreExec());

#ifdef __linux__
            guard(unix::expected([&] {
                return execvpe(program.c_str(), argv.get(), envp.get());
            }));
#else
            // ReSharper disable once CppDFALocalValueEscapesFunction
            environ = envp.get();
            guard(unix::expected([&] {
                return execvp(program.c_str(), argv.get());
            }));
#endif
        }

        error::guard(writer.close());

        int value{};
        const auto result = reader.readExactly({reinterpret_cast<std::byte *>(&value), sizeof(value)});

        if (result) {
            const auto id = error::guard(unix::ensure([&] {
                return waitpid(pid, nullptr, 0);
            }));
            assert(id == pid);

            return std::unexpected{std::error_code{value, std::system_category()}};
        }

        if (const auto &error = result.error(); error != io::Error::UnexpectedEOF)
            throw error::StacktraceError<std::system_error>{error};
    }
    else {
        const auto expected = []<std::invocable F>(F &&f) -> std::expected<void, std::error_code> {
            static_assert(std::is_same_v<std::invoke_result_t<F>, int>);
            const auto result = std::invoke(std::forward<F>(f));

            if (result != 0)
                return std::unexpected{std::error_code{result, std::generic_category()}};

            return {};
        };

        posix_spawn_file_actions_t actions{};

        error::guard(expected([&] {
            return posix_spawn_file_actions_init(&actions);
        }));

        Z_DEFER(error::guard(expected([&] {
            return posix_spawn_file_actions_destroy(&actions);
        })));

        for (int i{0}; i < 3; ++i) {
            if (const auto &resource = resources[indexMapping[i]]) {
                error::guard(expected([&] {
                    return posix_spawn_file_actions_adddup2(&actions, resource->fd(), i);
                }));
            }
#ifdef __APPLE__
            else {
                error::guard(expected([&] {
                    return posix_spawn_file_actions_addinherit_np(&actions, i);
                }));
            }
#endif
        }

        if (mCurrentDirectory) {
#if (defined(__ANDROID__) && __ANDROID_API__ < 34) || defined(__OHOS__)
            static const auto posix_spawn_file_actions_addchdir_np = reinterpret_cast<
                int (*)(posix_spawn_file_actions_t *, const char *)
            >(
                dlsym(RTLD_DEFAULT, "posix_spawn_file_actions_addchdir_np")
            );

            if (!posix_spawn_file_actions_addchdir_np)
                throw error::StacktraceError<std::runtime_error>{
                    "posix_spawn_file_actions_addchdir_np is not supported on this system"
                };
#endif
            error::guard(expected([&] {
                return posix_spawn_file_actions_addchdir_np(&actions, mCurrentDirectory->c_str());
            }));
        }

        posix_spawnattr_t attr{};

        error::guard(expected([&] {
            return posix_spawnattr_init(&attr);
        }));

        Z_DEFER(error::guard(expected([&] {
            return posix_spawnattr_destroy(&attr);
        })));

        {
            sigset_t set{};

            error::guard(expected([&] {
                return sigfillset(&set);
            }));

            error::guard(expected([&] {
                return posix_spawnattr_setsigdefault(&attr, &set);
            }));
        }

        {
            sigset_t set{};

            error::guard(expected([&] {
                return sigemptyset(&set);
            }));

            error::guard(expected([&] {
                return posix_spawnattr_setsigmask(&attr, &set);
            }));
        }

        if (mProcessGroup)
            error::guard(expected([&] {
                return posix_spawnattr_setpgroup(&attr, *mProcessGroup);
            }));

#ifdef __APPLE__
        {
            short flags = POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK;

            if (mProcessGroup)
                flags |= POSIX_SPAWN_SETPGROUP;

            error::guard(expected([&] {
                return posix_spawnattr_setflags(&attr, flags);
            }));
        }

        for (const auto &resource: mInheritedResources) {
            error::guard(expected([&] {
                return posix_spawn_file_actions_addinherit_np(&actions, *resource);
            }));
        }
#else
        {
            short flags = POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK;

            if (mProcessGroup)
                flags |= POSIX_SPAWN_SETPGROUP;

            error::guard(expected([&] {
                return posix_spawnattr_setflags(&attr, flags);
            }));
        }

        // The program assumes that the file descriptor to be inherited is not set FD_CLOEXEC.
        for (int fd{STDERR_FILENO + 1}; fd < fdLimit; ++fd) {
            if (std::ranges::find_if(
                mInheritedResources,
                [=](const auto &resource) {
                    return *resource == fd;
                }
            ) != mInheritedResources.end())
                continue;

            error::guard(expected([&] {
                return posix_spawn_file_actions_addclose(&actions, fd);
            }));
        }
#endif

        Z_EXPECT(expected([&] {
            return posix_spawnp(&pid, program.c_str(), &actions, &attr, argv.get(), envp.get());
        }));
    }

    auto process = open(pid);

    if (!process) {
        error::guard(unix::expected([&] {
            return kill(pid, SIGKILL);
        }).or_else([](const auto &ec) -> std::expected<int, std::error_code> {
            if (ec != std::errc::no_such_process)
                return std::unexpected{ec};

            return {};
        }));

        const auto id = error::guard(unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }));
        assert(id == pid);

        throw error::StacktraceError<std::system_error>{process.error()};
    }

    std::array<std::optional<IOResource>, 3> stdio;

    if (auto &resource = resources[StdinW])
        stdio[0] = *std::move(resource);

    if (auto &resource = resources[StdoutR])
        stdio[1] = *std::move(resource);

    if (auto &resource = resources[StderrR])
        stdio[2] = *std::move(resource);

    return ChildProcess{*std::move(process), std::move(stdio)};
#endif
}

std::expected<zero::os::process::ChildProcess, std::error_code> zero::os::process::Command::spawn() const {
    return spawn({Stdio::inherit(), Stdio::inherit(), Stdio::inherit()});
}

std::expected<zero::os::process::ExitStatus, std::error_code> zero::os::process::Command::status() const {
    return spawn({Stdio::inherit(), Stdio::inherit(), Stdio::inherit()}).transform(&ChildProcess::wait);
}

std::expected<zero::os::process::Output, std::error_code>
zero::os::process::Command::output() const {
    auto child = spawn({Stdio::null(), Stdio::piped(), Stdio::piped()});
    Z_EXPECT(child);

    if (auto input = std::exchange(child->stdInput(), std::nullopt))
        error::guard(input->close());

    auto future = std::async([&] {
        return child->stdError()
                    .transform(&io::IReader::readAll)
                    .value_or(std::vector<std::byte>{});
    });

    auto out = child->stdOutput()
                    .transform(&io::IReader::readAll)
                    .value_or(std::vector<std::byte>{});

    if (!out) {
        error::guard(
            child->kill().or_else([](const auto &ec) -> std::expected<void, std::error_code> {
#ifdef _WIN32
                if (ec != std::errc::permission_denied)
#else
                if (ec != std::errc::no_such_process)
#endif
                    return std::unexpected{ec};

                return {};
            })
        );
        child->wait();
        throw error::StacktraceError<std::system_error>{out.error()};
    }

    auto err = future.get();

    if (!err) {
        error::guard(
            child->kill().or_else([](const auto &ec) -> std::expected<void, std::error_code> {
#ifdef _WIN32
                if (ec != std::errc::permission_denied)
#else
                if (ec != std::errc::no_such_process)
#endif
                    return std::unexpected{ec};

                return {};
            })
        );
        child->wait();
        throw error::StacktraceError<std::system_error>{err.error()};
    }

    return Output{
        .status = child->wait(),
        .out = *std::move(out),
        .err = *std::move(err)
    };
}

#endif
