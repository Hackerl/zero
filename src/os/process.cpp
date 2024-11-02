#include <zero/os/process.h>
#include <zero/strings/strings.h>
#include <zero/expect.h>
#include <zero/defer.h>
#include <zero/env.h>
#include <fmt/ranges.h>
#include <cassert>
#include <ranges>
#include <future>

#ifdef _WIN32
#include <zero/os/windows/error.h>
#else
#include <csignal>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <zero/os/unix/error.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#if defined(__ANDROID__) && __ANDROID_API__ < 23
#include <dlfcn.h>
#endif
#endif
#endif

#ifdef __APPLE__
extern char **environ;
#endif

constexpr auto STDIN_R = 0;
constexpr auto STDIN_W = 1;
constexpr auto STDOUT_R = 2;
constexpr auto STDOUT_W = 3;
constexpr auto STDERR_R = 4;
constexpr auto STDERR_W = 5;

#ifdef _WIN32
static const auto createPseudoConsole = reinterpret_cast<decltype(&CreatePseudoConsole)>(
    GetProcAddress(GetModuleHandleA("kernel32"), "CreatePseudoConsole")
);

static const auto closePseudoConsole = reinterpret_cast<decltype(&ClosePseudoConsole)>(
    GetProcAddress(GetModuleHandleA("kernel32"), "ClosePseudoConsole")
);

static const auto resizePseudoConsole = reinterpret_cast<decltype(&ResizePseudoConsole)>(
    GetProcAddress(GetModuleHandleA("kernel32"), "ResizePseudoConsole")
);
#else
constexpr auto NOTIFY_R = 6;
constexpr auto NOTIFY_W = 7;
#endif

#ifdef _WIN32
static std::string quote(const std::string &arg) {
    std::string result;

    const auto str = arg.c_str();
    const auto quote = std::strchr(str, ' ') || std::strchr(str, '\t') || *str == '\0';

    if (quote)
        result.push_back('\"');

    int bsCount{0};

    for (const auto *p = str; *p != '\0'; ++p) {
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

static std::expected<std::array<HANDLE, 2>, std::error_code>
createPipe(const bool duplex, SECURITY_ATTRIBUTES *attributes = nullptr) {
    static std::atomic<std::size_t> number;
    const auto name = fmt::format(R"(\\?\pipe\zero\{}-{})", GetCurrentProcessId(), number++);

    auto first = CreateNamedPipeA(
        name.c_str(),
        (duplex ? PIPE_ACCESS_DUPLEX : PIPE_ACCESS_INBOUND) | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,
        65536,
        65536,
        0,
        attributes
    );

    if (first == INVALID_HANDLE_VALUE)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    DEFER(
        if (first)
            CloseHandle(first);
    );

    const auto second = CreateFileA(
        name.c_str(),
        (duplex ? GENERIC_READ : 0) | GENERIC_WRITE,
        0,
        attributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (second == INVALID_HANDLE_VALUE)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return std::array{std::exchange(first, nullptr), second};
}
#endif

zero::os::process::Process::Process(ProcessImpl impl): mImpl{std::move(impl)} {
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

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::process::Process::kill() {
#ifdef _WIN32
    return mImpl.terminate(EXIT_FAILURE);
#else
    return mImpl.kill(SIGKILL);
#endif
}

std::expected<zero::os::process::Process, std::error_code> zero::os::process::self() {
#ifdef _WIN32
    return windows::process::self()
#elif defined(__APPLE__)
    return macos::process::self()
#elif defined(__linux__)
    return linux::process::self()
#endif
        .transform([](auto process) {
            return Process{std::move(process)};
        });
}

std::expected<zero::os::process::Process, std::error_code> zero::os::process::open(const ID pid) {
#ifdef _WIN32
    return windows::process::open(pid)
#elif defined(__APPLE__)
    return macos::process::open(static_cast<pid_t>(pid))
#elif defined(__linux__)
    return linux::process::open(static_cast<pid_t>(pid))
#endif
        .transform([](auto process) {
            return Process{std::move(process)};
        });
}

std::expected<std::list<zero::os::process::ID>, std::error_code> zero::os::process::all() {
#ifdef _WIN32
    return windows::process::all()
#elif defined(__APPLE__)
    return macos::process::all()
#elif defined(__linux__)
    return linux::process::all()
#endif
        .transform([](const auto &ids) {
            return ids
                | std::views::transform([](const auto &pid) {
                    return static_cast<ID>(pid);
                })
                | std::ranges::to<std::list>();
        });
}

zero::os::process::ExitStatus::ExitStatus(const Status status) : mStatus{status} {
}

zero::os::process::ExitStatus::Status zero::os::process::ExitStatus::raw() const {
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

zero::os::process::ChildProcess::ChildProcess(Process process, const std::array<std::optional<StdioFile>, 3> &stdio)
    : Process{std::move(process)}, mStdio{stdio} {
}

zero::os::process::ChildProcess::ChildProcess(ChildProcess &&rhs) noexcept
    : Process{std::move(rhs)}, mStdio{std::exchange(rhs.mStdio, {})} {
}

zero::os::process::ChildProcess &zero::os::process::ChildProcess::operator=(ChildProcess &&rhs) noexcept {
    Process::operator=(std::move(rhs));
    mStdio = std::exchange(rhs.mStdio, {});
    return *this;
}

zero::os::process::ChildProcess::~ChildProcess() {
    for (const auto &fd: mStdio) {
        if (!fd)
            continue;

#ifdef _WIN32
        CloseHandle(*fd);
#else
        close(*fd);
#endif
    }
}

std::optional<zero::os::process::ChildProcess::StdioFile> &zero::os::process::ChildProcess::stdInput() {
    return mStdio[0];
}

std::optional<zero::os::process::ChildProcess::StdioFile> &zero::os::process::ChildProcess::stdOutput() {
    return mStdio[1];
}

std::optional<zero::os::process::ChildProcess::StdioFile> &zero::os::process::ChildProcess::stdError() {
    return mStdio[2];
}

#ifdef _WIN32
zero::os::process::PseudoConsole::PseudoConsole(const HPCON pc, const std::array<HANDLE, 3> &handles)
    : mPC{pc}, mHandles{handles} {
}

zero::os::process::PseudoConsole::PseudoConsole(PseudoConsole &&rhs) noexcept
    : mPC{std::exchange(rhs.mPC, nullptr)}, mHandles{std::exchange(rhs.mHandles, {})} {
}

zero::os::process::PseudoConsole &zero::os::process::PseudoConsole::operator=(PseudoConsole &&rhs) noexcept {
    mPC = std::exchange(rhs.mPC, nullptr);
    mHandles = std::exchange(rhs.mHandles, {});
    return *this;
}

zero::os::process::PseudoConsole::~PseudoConsole() {
    if (!mPC)
        return;

    closePseudoConsole(mPC);

    for (const auto handle: mHandles) {
        if (!handle)
            continue;

        CloseHandle(handle);
    }
}

std::expected<zero::os::process::PseudoConsole, std::error_code>
zero::os::process::PseudoConsole::make(const short rows, const short columns) {
    if (!createPseudoConsole || !closePseudoConsole || !resizePseudoConsole)
        return std::unexpected{Error::API_NOT_AVAILABLE};

    const auto pipes = createPipe(true);
    EXPECT(pipes);

    std::array<HANDLE, 3> handles{pipes->at(0), pipes->at(1)};

    DEFER(
        for (const auto &handle: handles) {
            if (!handle)
                continue;

            CloseHandle(handle);
        }
    );

    EXPECT(windows::expected([&] {
        return DuplicateHandle(
            GetCurrentProcess(),
            handles[1],
            GetCurrentProcess(),
            handles.data() + 2,
            0,
            false,
            DUPLICATE_SAME_ACCESS
        );
    }));

    HPCON hPC{};

    if (const auto result = createPseudoConsole(
        {columns, rows},
        handles[1],
        handles[2],
        0,
        &hPC
    ); result != S_OK)
        return std::unexpected{static_cast<windows::ResultHandle>(result)};

    return PseudoConsole{hPC, std::exchange(handles, {})};
}

void zero::os::process::PseudoConsole::close() {
    assert(mPC);
    closePseudoConsole(std::exchange(mPC, nullptr));
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    if (const auto result = resizePseudoConsole(mPC, {columns, rows}); result != S_OK)
        return std::unexpected{static_cast<windows::ResultHandle>(result)};

    return {};
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::PseudoConsole::spawn(const Command &command) {
    assert(mHandles[1]);
    assert(mHandles[2]);

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(siEx);
    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    EXPECT(windows::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
    }));

    EXPECT(windows::expected([&] {
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

    if (command.mInheritEnv) {
        auto result = env::list();
        EXPECT(result);
        envs = *std::move(result);
    }

    for (const auto &[key, value]: command.mEnviron) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = strings::decode(to_string(fmt::join(
        envs | std::views::transform([](const auto &it) {
            auto env = it.first + "=" + it.second;
            env.push_back('\0');
            return env;
        }),
        ""
    )));
    EXPECT(environment);

    std::vector arguments{command.mPath.string()};
    std::ranges::copy(command.mArguments, std::back_inserter(arguments));

    auto cmd = strings::decode(to_string(
        fmt::join(arguments | std::views::transform(quote), " ")
    ));
    EXPECT(cmd);

    PROCESS_INFORMATION info{};

    EXPECT(windows::expected([&] {
        return CreateProcessW(
            nullptr,
            cmd->data(),
            nullptr,
            nullptr,
            false,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            environment->data(),
            command.mCurrentDirectory ? command.mCurrentDirectory->wstring().c_str() : nullptr,
            &siEx.StartupInfo,
            &info
        );
    }));

    CloseHandle(info.hThread);
    CloseHandle(std::exchange(mHandles[1], nullptr));
    CloseHandle(std::exchange(mHandles[2], nullptr));

    return ChildProcess{Process{windows::process::Process{info.hProcess, info.dwProcessId}}, {}};
}

HANDLE &zero::os::process::PseudoConsole::file() {
    return mHandles[0];
}
#else
zero::os::process::PseudoConsole::PseudoConsole(const int master, const int slave): mMaster{master}, mSlave{slave} {
}

zero::os::process::PseudoConsole::PseudoConsole(PseudoConsole &&rhs) noexcept
    : mMaster{std::exchange(rhs.mMaster, -1)}, mSlave{std::exchange(rhs.mSlave, -1)} {
}

zero::os::process::PseudoConsole &zero::os::process::PseudoConsole::operator=(PseudoConsole &&rhs) noexcept {
    mMaster = std::exchange(rhs.mMaster, -1);
    mSlave = std::exchange(rhs.mSlave, -1);
    return *this;
}

zero::os::process::PseudoConsole::~PseudoConsole() {
    if (mMaster >= 0)
        close(mMaster);

    if (mSlave >= 0)
        close(mSlave);
}

std::expected<zero::os::process::PseudoConsole, std::error_code>
zero::os::process::PseudoConsole::make(const short rows, const short columns) {
#if defined(__ANDROID__) && __ANDROID_API__ < 23
    static const auto openpty = reinterpret_cast<int (*)(int *, int *, char *, const termios *, const winsize *)>(
        dlsym(RTLD_DEFAULT, "openpty")
    );

    if (!openpty)
        return std::unexpected{Error::API_NOT_AVAILABLE};
#endif
    int master{}, slave{};

    EXPECT(unix::expected([&] {
        return openpty(&master, &slave, nullptr, nullptr, nullptr);
    }));

    PseudoConsole pc{master, slave};
    EXPECT(pc.resize(rows, columns));

    return pc;
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    winsize ws{};

    ws.ws_row = rows;
    ws.ws_col = columns;

    EXPECT(unix::expected([&] {
        return ioctl(mMaster, TIOCSWINSZ, &ws);
    }));

    return {};
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::PseudoConsole::spawn(const Command &command) {
    std::array<int, 2> fds{};

    EXPECT(unix::expected([&] {
        return pipe(fds.data());
    }));

    DEFER(
        for (const auto &fd: fds) {
            if (fd < 0)
                continue;

            close(fd);
        }
    );

    assert(mSlave > STDERR_FILENO);
    assert(std::ranges::all_of(fds, [](const auto &fd) {return fd > STDERR_FILENO;}));

    EXPECT(unix::expected([&] {
        return fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    }));

    const auto program = command.mPath.string();

    std::vector arguments{program};
    std::ranges::copy(command.mArguments, std::back_inserter(arguments));

    std::map<std::string, std::string> envs;

    if (command.mInheritEnv) {
        auto result = env::list();
        EXPECT(result);
        envs = *std::move(result);
    }

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
        [](const auto &it) {
            return fmt::format("{}={}", it.first, it.second);
        }
    );

    const auto argv = std::make_unique<char *[]>(arguments.size() + 1);
    const auto envp = std::make_unique<char *[]>(environment.size() + 1);

    std::ranges::transform(arguments, argv.get(), [](auto &str) {
        return str.data();
    });

    std::ranges::transform(environment, envp.get(), [](auto &str) {
        return str.data();
    });

    const auto pid = unix::expected([] {
        return fork();
    });
    EXPECT(pid);

    if (*pid == 0) {
        if (setsid() == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

        if (ioctl(mSlave, TIOCSCTTY, nullptr) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

        if (dup2(mSlave, STDIN_FILENO) == -1 || dup2(mSlave, STDOUT_FILENO) == -1 || dup2(mSlave, STDERR_FILENO) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

        const auto max = static_cast<int>(sysconf(_SC_OPEN_MAX));

        if (max == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

        for (int i{STDERR_FILENO + 1}; i < max; ++i) {
            if (i == fds[1])
                continue;

            close(i);
        }

        if (command.mCurrentDirectory && chdir(command.mCurrentDirectory->string().c_str()) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

#ifdef __linux__
        if (execvpe(program.data(), argv.get(), envp.get()) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }
#else
        environ = envp.get();

        if (execvp(program.data(), argv.get()) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }
#endif
    }

    close(std::exchange(fds[1], -1));

    int error{};

    const auto n = unix::ensure([&] {
        return read(fds[0], &error, sizeof(error));
    });
    assert(n);

    if (*n != 0) {
        assert(n == sizeof(int));
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == pid);
        return std::unexpected{std::error_code{error, std::system_category()}};
    }

    close(std::exchange(mSlave, -1));

    auto process = open(*pid);

    if (!process) {
        kill(*pid, SIGKILL);
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == *pid);
        return std::unexpected{process.error()};
    }

    return ChildProcess{*std::move(process), {}};
}

int &zero::os::process::PseudoConsole::file() {
    return mMaster;
}
#endif

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<zero::os::process::ExitStatus, std::error_code> zero::os::process::ChildProcess::wait() {
#ifdef _WIN32
    const auto &impl = this->impl();

    EXPECT(impl.wait(std::nullopt));
    return impl.exitCode().transform([](const DWORD code) {
        return ExitStatus{code};
    });
#else
    int s{};
    const auto pid = this->impl().pid();

    const auto id = unix::ensure([&] {
        return waitpid(pid, &s, 0);
    });
    EXPECT(id);
    assert(*id == pid);

    return ExitStatus{s};
#endif
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<std::optional<zero::os::process::ExitStatus>, std::error_code>
zero::os::process::ChildProcess::tryWait() {
#ifdef _WIN32
    using namespace std::chrono_literals;

    const auto &impl = this->impl();

    if (const auto result = impl.wait(0ms); !result) {
        if (result.error() == std::errc::timed_out)
            return std::nullopt;

        return std::unexpected{result.error()};
    }

    return impl.exitCode().transform([](const DWORD code) {
        return ExitStatus{code};
    });
#else
    int s{};
    const auto pid = this->impl().pid();

    const auto id = unix::expected([&] {
        return waitpid(pid, &s, WNOHANG);
    });
    EXPECT(id);

    if (*id == 0)
        return std::nullopt;

    return ExitStatus{s};
#endif
}

zero::os::process::Command::Command(std::filesystem::path path): mInheritEnv{true}, mPath{std::move(path)} {
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

zero::os::process::Command &zero::os::process::Command::arg(std::string arg) {
    mArguments.push_back(std::move(arg));
    return *this;
}

zero::os::process::Command &zero::os::process::Command::args(std::vector<std::string> args) {
    mArguments = std::move(args);
    return *this;
}

zero::os::process::Command &zero::os::process::Command::currentDirectory(std::filesystem::path path) {
    mCurrentDirectory = std::move(path);
    return *this;
}

zero::os::process::Command &zero::os::process::Command::env(std::string key, std::string value) {
    mEnviron[std::move(key)] = std::move(value);
    return *this;
}

zero::os::process::Command &zero::os::process::Command::envs(std::map<std::string, std::string> envs) {
    for (auto &[key, value]: envs)
        mEnviron[key] = std::move(value);

    return *this;
}

zero::os::process::Command &zero::os::process::Command::clearEnv() {
    mInheritEnv = false;
    mEnviron.clear();
    return *this;
}

zero::os::process::Command &zero::os::process::Command::removeEnv(const std::string &key) {
    if (!mInheritEnv) {
        mEnviron.erase(key);
        return *this;
    }

    mEnviron[key] = std::nullopt;
    return *this;
}

zero::os::process::Command &zero::os::process::Command::stdInput(const StdioType type) {
    mStdioTypes[0] = type;
    return *this;
}

zero::os::process::Command &zero::os::process::Command::stdOutput(const StdioType type) {
    mStdioTypes[1] = type;
    return *this;
}

zero::os::process::Command &zero::os::process::Command::stdError(const StdioType type) {
    mStdioTypes[2] = type;
    return *this;
}

std::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::Command::spawn(const std::array<StdioType, 3> &defaultTypes) const {
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr{};

    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = true;
    saAttr.lpSecurityDescriptor = nullptr;

    std::array<HANDLE, 6> handles{};

    DEFER(
        for (const auto &handle: handles) {
            if (!handle)
                continue;

            CloseHandle(handle);
        }
    );

    constexpr std::array indexMapping{STDIN_R, STDOUT_W, STDERR_W};
    constexpr std::array typeMapping{STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    constexpr std::array<DWORD, 3> accessMapping{GENERIC_READ, GENERIC_WRITE, GENERIC_WRITE};

    for (int i{0}; i < 3; ++i) {
        const auto type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == StdioType::INHERIT) {
            const auto handle = GetStdHandle(typeMapping[i]);

            if (!handle)
                continue;

            EXPECT(windows::expected([&] {
                return DuplicateHandle(
                    GetCurrentProcess(),
                    handle,
                    GetCurrentProcess(),
                    handles.data() + indexMapping[i],
                    0,
                    true,
                    DUPLICATE_SAME_ACCESS
                );
            }));
            continue;
        }

        if (type == StdioType::PIPED) {
            const auto pipes = createPipe(false, &saAttr);
            EXPECT(pipes);

            handles[i * 2] = pipes->at(0);
            handles[i * 2 + 1] = pipes->at(1);
            continue;
        }

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
            return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

        handles[indexMapping[i]] = handle;
    }

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(siEx);

    constexpr std::array memberPointers{
        &STARTUPINFOW::hStdInput,
        &STARTUPINFOW::hStdOutput,
        &STARTUPINFOW::hStdError
    };

    std::vector<HANDLE> inheritedHandles;

    for (int i{0}; i < 3; ++i) {
        const auto handle = handles[indexMapping[i]];

        if (!handle)
            continue;

        siEx.StartupInfo.*memberPointers[i] = handle;
        inheritedHandles.push_back(handle);
    }

    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T size{};
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    EXPECT(windows::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
    }));

    EXPECT(windows::expected([&] {
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

    if (mInheritEnv) {
        auto result = env::list();
        EXPECT(result);
        envs = *std::move(result);
    }

    for (const auto &[key, value]: mEnviron) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = strings::decode(to_string(fmt::join(
        envs | std::views::transform([](const auto &it) {
            auto env = it.first + "=" + it.second;
            env.push_back('\0');
            return env;
        }),
        ""
    )));
    EXPECT(environment);

    std::vector arguments{mPath.string()};
    std::ranges::copy(mArguments, std::back_inserter(arguments));

    auto cmd = strings::decode(to_string(
        fmt::join(arguments | std::views::transform(quote), " ")
    ));
    EXPECT(cmd);

    PROCESS_INFORMATION info{};

    EXPECT(windows::expected([&] {
        return CreateProcessW(
            nullptr,
            cmd->data(),
            nullptr,
            nullptr,
            true,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            environment->data(),
            mCurrentDirectory ? mCurrentDirectory->wstring().c_str() : nullptr,
            &siEx.StartupInfo,
            &info
        );
    }));

    CloseHandle(info.hThread);

    std::array<std::optional<ChildProcess::StdioFile>, 3> stdio;

    if (handles[STDIN_W])
        stdio[0] = std::exchange(handles[STDIN_W], nullptr);

    if (handles[STDOUT_R])
        stdio[1] = std::exchange(handles[STDOUT_R], nullptr);

    if (handles[STDERR_R])
        stdio[2] = std::exchange(handles[STDERR_R], nullptr);

    return ChildProcess{Process{windows::process::Process{info.hProcess, info.dwProcessId}}, stdio};
#else
    std::array<int, 8> fds{};
    std::ranges::fill(fds, -1);

    DEFER(
        for (const auto &fd: fds) {
            if (fd < 0)
                continue;

            close(fd);
        }
    );

    EXPECT(unix::expected([&] {
        return pipe(fds.data() + NOTIFY_R);
    }));

    EXPECT(unix::expected([&] {
        return fcntl(fds[NOTIFY_W], F_SETFD, FD_CLOEXEC);
    }));

    constexpr std::array indexMapping{0, 3, 5};
    constexpr std::array flagMapping{O_RDONLY, O_WRONLY, O_WRONLY};

    for (int i{0}; i < 3; ++i) {
        const auto type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == StdioType::INHERIT)
            continue;

        if (type == StdioType::PIPED) {
            EXPECT(unix::expected([&] {
                return pipe(fds.data() + i * 2);
            }));
            continue;
        }

        const auto fd = unix::expected([&] {
            return ::open("/dev/null", flagMapping[i]);
        });
        EXPECT(fd);

        fds[indexMapping[i]] = *fd;
    }

    assert(std::ranges::all_of(fds, [](const auto &fd) {return fd == -1 || fd > STDERR_FILENO;}));

    const auto program = mPath.string();

    std::vector arguments{program};
    std::ranges::copy(mArguments, std::back_inserter(arguments));

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        auto result = env::list();
        EXPECT(result);
        envs = *std::move(result);
    }

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
        [](const auto &it) {
            return fmt::format("{}={}", it.first, it.second);
        }
    );

    const auto argv = std::make_unique<char *[]>(arguments.size() + 1);
    const auto envp = std::make_unique<char *[]>(environment.size() + 1);

    std::ranges::transform(arguments, argv.get(), [](auto &str) {
        return str.data();
    });

    std::ranges::transform(environment, envp.get(), [](auto &str) {
        return str.data();
    });

    const auto pid = unix::expected([] {
        return fork();
    });
    EXPECT(pid);

    if (*pid == 0) {
        if ((fds[STDIN_R] > 0 && dup2(fds[STDIN_R], STDIN_FILENO) == -1)
            || (fds[STDOUT_W] > 0 && dup2(fds[STDOUT_W], STDOUT_FILENO) == -1)
            || (fds[STDERR_W] > 0 && dup2(fds[STDERR_W], STDERR_FILENO) == -1)) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_W], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

        const auto max = static_cast<int>(sysconf(_SC_OPEN_MAX));

        if (max == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

        for (int i{STDERR_FILENO + 1}; i < max; ++i) {
            if (i == fds[NOTIFY_W])
                continue;

            close(i);
        }

        if (mCurrentDirectory && chdir(mCurrentDirectory->string().c_str()) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_W], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }

#ifdef __linux__
        if (execvpe(program.data(), argv.get(), envp.get()) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_W], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }
#else
        environ = envp.get();

        if (execvp(program.data(), argv.get()) == -1) {
            const auto error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_W], &error, sizeof(error));
            });
            assert(n);
            std::abort();
        }
#endif
    }

    close(std::exchange(fds[NOTIFY_W], -1));

    int error{};

    const auto n = unix::ensure([&] {
        return read(fds[NOTIFY_R], &error, sizeof(error));
    });
    assert(n);

    if (*n != 0) {
        assert(n == sizeof(int));
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == pid);
        return std::unexpected{std::error_code{error, std::system_category()}};
    }

    auto process = open(*pid);

    if (!process) {
        kill(*pid, SIGKILL);
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == pid);
        return std::unexpected{process.error()};
    }

    std::array<std::optional<ChildProcess::StdioFile>, 3> stdio;

    if (fds[STDIN_W] >= 0)
        stdio[0] = std::exchange(fds[STDIN_W], -1);

    if (fds[STDOUT_R] >= 0)
        stdio[1] = std::exchange(fds[STDOUT_R], -1);

    if (fds[STDERR_R] >= 0)
        stdio[2] = std::exchange(fds[STDERR_R], -1);

    return ChildProcess{*std::move(process), stdio};
#endif
}

std::expected<zero::os::process::ChildProcess, std::error_code> zero::os::process::Command::spawn() const {
    return spawn({StdioType::INHERIT, StdioType::INHERIT, StdioType::INHERIT});
}

std::expected<zero::os::process::ExitStatus, std::error_code> zero::os::process::Command::status() const {
    return spawn({StdioType::INHERIT, StdioType::INHERIT, StdioType::INHERIT}).and_then(&ChildProcess::wait);
}

std::expected<zero::os::process::Output, std::error_code>
zero::os::process::Command::output() const {
    auto child = spawn({StdioType::NUL, StdioType::PIPED, StdioType::PIPED});
    EXPECT(child);

    if (const auto input = std::exchange(child->stdInput(), std::nullopt)) {
#ifdef _WIN32
        CloseHandle(*input);
#else
        close(*input);
#endif
    }

    const auto readAll = [](const auto &fd) -> std::expected<std::vector<std::byte>, std::error_code> {
        if (!fd)
            return {};

        std::vector<std::byte> data;

#ifdef _WIN32
        while (true) {
            DWORD n{};
            std::array<std::byte, 1024> buffer; // NOLINT(*-pro-type-member-init)

            if (const auto result = windows::expected([&] {
                return ReadFile(*fd, buffer.data(), buffer.size(), &n, nullptr);
            }); !result) {
                if (result.error() != std::errc::broken_pipe)
                    return std::unexpected{result.error()};

                break;
            }

            assert(n > 0);
            data.insert(data.end(), buffer.begin(), buffer.begin() + n);
        }
#else
        while (true) {
            std::array<std::byte, 1024> buffer; // NOLINT(*-pro-type-member-init)

            const auto n = unix::ensure([&] {
                return read(*fd, buffer.data(), buffer.size());
            });
            EXPECT(n);

            if (*n == 0)
                break;

            data.insert(data.end(), buffer.begin(), buffer.begin() + *n);
        }
#endif

        return data;
    };

    auto future = std::async(readAll, child->stdError());
    auto out = readAll(child->stdOutput());

    if (!out) {
        std::ignore = child->kill();
        std::ignore = child->wait();
        return std::unexpected{out.error()};
    }

    auto err = future.get();

    if (!err) {
        std::ignore = child->kill();
        std::ignore = child->wait();
        return std::unexpected{err.error()};
    }

    const auto status = child->wait();
    EXPECT(status);

    return Output{
        *status,
        *std::move(out),
        *std::move(err)
    };
}

#if defined(_WIN32) || (defined(__ANDROID__) && __ANDROID_API__ < 23)
DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::process::PseudoConsole::Error)
#endif
