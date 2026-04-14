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
constexpr auto FDMax = 65535;
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
        return CPUTime{cpu.user, cpu.system};
    });
}

std::expected<zero::os::process::MemoryStat, std::error_code> zero::os::process::Process::memory() const {
    return mImpl.memory().transform([](const auto &memory) {
        return MemoryStat{memory.rss, memory.vms};
    });
}

std::expected<zero::os::process::IOStat, std::error_code> zero::os::process::Process::io() const {
    return mImpl.io().transform([](const auto &io) {
        return IOStat{io.readBytes, io.writeBytes};
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
#elif defined(__APPLE__)
    return Process{macos::process::self()};
#elif defined(__linux__)
    return Process{linux::process::self()};
#endif
}

std::expected<zero::os::process::Process, std::error_code> zero::os::process::open(const ID pid) {
#ifdef _WIN32
    return windows::process::open(pid)
#elif defined(__APPLE__)
    return macos::process::open(static_cast<pid_t>(pid))
#elif defined(__linux__)
    return linux::process::open(static_cast<pid_t>(pid))
#endif
        .transform([](ProcessImpl &&process) {
            return Process{std::move(process)};
        });
}

std::list<zero::os::process::ID> zero::os::process::all() {
#ifdef _WIN32
    return windows::process::all()
#elif defined(__APPLE__)
    return macos::process::all()
#elif defined(__linux__)
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
        {columns, rows},
        inReader.fd(),
        outWriter.fd(),
        0,
        &hPC
    ); result != S_OK)
        return std::unexpected{static_cast<windows::ResultHandle>(result)};

    return PseudoConsole{
        hPC,
        {std::move(outReader), std::move(inWriter)},
        {std::move(inReader), std::move(outWriter)}
    };
}

void zero::os::process::PseudoConsole::close() {
    assert(mPC);
    closePseudoConsole(std::exchange(mPC, nullptr));
}

// ReSharper disable once CppMemberFunctionMayBeConst
void zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    if (const auto result = resizePseudoConsole(mPC, {columns, rows}); result != S_OK)
        throw error::StacktraceError<std::system_error>{static_cast<windows::ResultHandle>(result)};
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::PseudoConsole::spawn(const Command &command) {
    assert(mSlave.reader);
    assert(mSlave.writer);
    assert(command.mInheritedResources.empty());
    assert(command.mInheritedNativeResources.empty());

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(siEx);
    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    error::guard(windows::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
    }));

    error::guard(windows::expected([&] {
        return UpdateProcThreadAttribute(
            siEx.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            mPC,
            sizeof(mPC),
            nullptr,
            nullptr
        );
    }));

    std::map<std::string, std::string> envs;

    if (command.mInheritEnv)
        envs = env::list();

    for (const auto &[key, value]: command.mEnviron) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = error::guard(
        strings::decode(to_string(fmt::join(
            envs | std::views::transform([](const auto &pair) {
                auto env = pair.first + "=" + pair.second;
                env.push_back('\0');
                return env;
            }),
            ""
        )))
    );

    std::vector arguments{filesystem::stringify(command.mPath)};
    arguments.append_range(command.mArguments);

    auto cmd = error::guard(
        strings::decode(to_string(
            fmt::join(arguments | std::views::transform(quote), " ")
        ))
    );

    PROCESS_INFORMATION info{};

    Z_EXPECT(windows::expected([&] {
        return CreateProcessW(
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            false,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            environment.data(),
            command.mCurrentDirectory ? command.mCurrentDirectory->wstring().c_str() : nullptr,
            &siEx.StartupInfo,
            &info
        );
    }));

    error::guard(windows::expected([&] {
        return CloseHandle(info.hThread);
    }));

    error::guard(mSlave.reader.close());
    error::guard(mSlave.writer.close());

    return ChildProcess{Process{windows::process::Process{Resource{info.hProcess}, info.dwProcessId}}, {}};
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
zero::os::process::PseudoConsole::spawn(const Command &command) {
    assert(command.mInheritedResources.empty());
    assert(command.mInheritedNativeResources.empty());
    assert(mSlave.fd() > STDERR_FILENO);

    auto [reader, writer] = pipe();
    writer.setInheritable(false);

    const auto &program = command.mPath.native();

    std::vector arguments{program};
    arguments.append_range(command.mArguments);

    std::map<std::string, std::string> envs;

    if (command.mInheritEnv)
        envs = env::list();

    for (const auto &[key, value]: command.mEnviron) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    std::vector<std::string> environment;
    std::ranges::transform(
        envs,
        std::back_inserter(environment),
        [](const auto &pair) {
            return fmt::format("{}={}", pair.first, pair.second);
        }
    );

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

    const auto pid = error::guard(unix::expected(fork));

    if (pid == 0) {
        const auto guard = [fd = writer.fd()]<std::invocable F>(F &&f) {
            static_assert(std::is_integral_v<std::invoke_result_t<F>>);
            const auto result = std::invoke(std::forward<F>(f));

            if (result == -1) {
                const auto error = errno;
                const auto n = unix::ensure([&] {
                    return write(fd, &error, sizeof(error));
                });
                assert(n);
                assert(*n == sizeof(error));
                std::abort();
            }

            return result;
        };

        for (int n{1}; n < 32; ++n) {
            if (n == SIGKILL || n == SIGSTOP)
                continue;

            guard([&] {
                return static_cast<int>(reinterpret_cast<std::intptr_t>(signal(n, SIG_DFL)));
            });
        }

        guard([] {
            return setsid();
        });

        guard([this] {
            return ioctl(mSlave.fd(), TIOCSCTTY, nullptr);
        });

        guard([this] {
            return dup2(mSlave.fd(), STDIN_FILENO);
        });

        guard([this] {
            return dup2(mSlave.fd(), STDOUT_FILENO);
        });

        guard([this] {
            return dup2(mSlave.fd(), STDERR_FILENO);
        });

        errno = 0;
        auto max = sysconf(_SC_OPEN_MAX);

        if (max == -1) {
            if (const auto error = errno; error != 0) {
                const auto n = unix::ensure([&] {
                    return write(writer.fd(), &error, sizeof(error));
                });
                assert(n);
                assert(*n == sizeof(error));
                std::abort();
            }

            max = FDMax;
        }

        for (int fd{STDERR_FILENO + 1}; fd < std::min(static_cast<int>(max), FDMax); ++fd) {
            if (fd == writer.fd())
                continue;

            std::ignore = unix::expected([&] {
                return close(fd);
            });
        }

        if (const auto &directory = command.mCurrentDirectory) {
            guard([&] {
                return chdir(directory->string().c_str());
            });
        }

        sigset_t set{};

        guard([&] {
            return sigemptyset(&set);
        });

        guard([&] {
            return sigprocmask(SIG_SETMASK, &set, nullptr);
        });

#ifdef __linux__
        guard([&] {
            return execvpe(program.c_str(), argv.get(), envp.get());
        });
#else
        environ = envp.get();
        guard([&] {
            return execvp(program.c_str(), argv.get());
        });
#endif
    }

    error::guard(writer.close());

    int error{};

    if (const auto n = error::guard(reader.read({reinterpret_cast<std::byte *>(&error), sizeof(error)})); n != 0) {
        assert(n == sizeof(int));

        const auto id = error::guard(unix::ensure([&] {
            return waitpid(pid, nullptr, 0);
        }));
        assert(id == pid);

        return std::unexpected{std::error_code{error, std::system_category()}};
    }

    error::guard(mSlave.close());

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

    return ChildProcess{*std::move(process), {}};
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

const std::vector<zero::os::Resource::Native> &zero::os::process::Command::inheritedNativeResources() const {
    return mInheritedNativeResources;
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::Command::spawn(const std::array<StdioType, 3> &defaultTypes) const {
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
        const auto type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == StdioType::Inherit) {
            const auto handle = GetStdHandle(typeMapping[i]);

            if (handle == INVALID_HANDLE_VALUE)
                throw error::StacktraceError<std::system_error>{static_cast<int>(GetLastError()), std::system_category()};

            if (!handle)
                continue;

            HANDLE duplicate{};

            error::guard(windows::expected([&] {
                return DuplicateHandle(
                    GetCurrentProcess(),
                    handle,
                    GetCurrentProcess(),
                    &duplicate,
                    0,
                    true,
                    DUPLICATE_SAME_ACCESS
                );
            }));

            resources[indexMapping[i]].emplace(duplicate);
            continue;
        }

        if (type == StdioType::Piped) {
            auto [reader, writer] = pipe();

            reader.setInheritable(true);
            writer.setInheritable(true);

            resources[i * 2] = std::move(reader);
            resources[i * 2 + 1] = std::move(writer);
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

    inheritedHandles.append_range(mInheritedNativeResources);

    for (int i{0}; i < 3; ++i) {
        const auto &resource = resources[indexMapping[i]];

        if (!resource)
            continue;

        const auto fd = resource->fd();
        siEx.StartupInfo.*memberPointers[i] = fd;
        inheritedHandles.push_back(fd);
    }

    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    error::guard(windows::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
    }));

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
        strings::decode(to_string(fmt::join(
            envs | std::views::transform([](const auto &pair) {
                auto env = pair.first + "=" + pair.second;
                env.push_back('\0');
                return env;
            }),
            ""
        )))
    );

    std::vector arguments{filesystem::stringify(mPath)};
    arguments.append_range(mArguments);

    auto cmd = error::guard(
        strings::decode(to_string(
            fmt::join(arguments | std::views::transform(quote), " ")
        ))
    );

    PROCESS_INFORMATION info{};

    Z_EXPECT(windows::expected([&] {
        return CreateProcessW(
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            true,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
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
        const auto type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == StdioType::Inherit)
            continue;

        if (type == StdioType::Piped) {
            auto [reader, writer] = pipe();

            resources[i * 2] = std::move(reader);
            resources[i * 2 + 1] = std::move(writer);
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

    std::vector arguments{program};
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

    std::vector<std::string> environment;
    std::ranges::transform(
        envs,
        std::back_inserter(environment),
        [](const auto &pair) {
            return fmt::format("{}={}", pair.first, pair.second);
        }
    );

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

#ifdef __APPLE__
    error::guard(expected([&] {
        return posix_spawnattr_setflags(
            &attr,
            POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK
        );
    }));

    for (const auto &resource: mInheritedResources) {
        error::guard(expected([&] {
            return posix_spawn_file_actions_addinherit_np(&actions, *resource);
        }));
    }

    for (const auto &resource: mInheritedNativeResources) {
        error::guard(expected([&] {
            return posix_spawn_file_actions_addinherit_np(&actions, resource);
        }));
    }
#else
    error::guard(expected([&] {
        return posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
    }));

    errno = 0;
    auto max = sysconf(_SC_OPEN_MAX);

    if (max == -1) {
        if (errno != 0)
            throw error::StacktraceError<std::system_error>{errno, std::system_category()};

        max = FDMax;
    }

    // The program assumes that the file descriptor to be inherited is not set FD_CLOEXEC.
    for (int fd{STDERR_FILENO + 1}; fd < std::min(static_cast<int>(max), FDMax); ++fd) {
        if (std::ranges::find_if(
            mInheritedResources,
            [=](const auto &resource) {
                return *resource == fd;
            }
        ) != mInheritedResources.end())
            continue;

        if (std::ranges::find(mInheritedNativeResources, fd) != mInheritedNativeResources.end())
            continue;

        error::guard(expected([&] {
            return posix_spawn_file_actions_addclose(&actions, fd);
        }));
    }
#endif

    pid_t pid{};

    Z_EXPECT(expected([&] {
        return posix_spawnp(&pid, program.c_str(), &actions, &attr, argv.get(), envp.get());
    }));

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
    return spawn({StdioType::Inherit, StdioType::Inherit, StdioType::Inherit});
}

std::expected<zero::os::process::ExitStatus, std::error_code> zero::os::process::Command::status() const {
    return spawn({StdioType::Inherit, StdioType::Inherit, StdioType::Inherit}).transform(&ChildProcess::wait);
}

std::expected<zero::os::process::Output, std::error_code>
zero::os::process::Command::output() const {
    auto child = spawn({StdioType::Null, StdioType::Piped, StdioType::Piped});
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
        child->wait(),
        *std::move(out),
        *std::move(err)
    };
}

#endif
