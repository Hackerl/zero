#include <zero/os/process.h>
#include <zero/strings/strings.h>
#include <zero/expect.h>
#include <zero/defer.h>
#include <range/v3/all.hpp>
#include <fmt/ranges.h>
#include <cassert>
#include <future>

#ifdef _WIN32
#include <zero/os/nt/error.h>
#else
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <zero/os/unix/error.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#if __ANDROID__ && __ANDROID_API__ < 23
#include <dlfcn.h>
#endif
#endif
#endif

#ifdef __APPLE__
extern char **environ;
#endif

constexpr auto STDIN_READER = 0;
constexpr auto STDIN_WRITER = 1;
constexpr auto STDOUT_READER = 2;
constexpr auto STDOUT_WRITER = 3;
constexpr auto STDERR_READER = 4;
constexpr auto STDERR_WRITER = 5;

#ifdef _WIN32
constexpr auto PTY_MASTER_READER = 0;
constexpr auto PTY_SLAVE_WRITER = 1;
constexpr auto PTY_SLAVE_READER = 2;
constexpr auto PTY_MASTER_WRITER = 3;

static const auto createPseudoConsole = reinterpret_cast<decltype(CreatePseudoConsole) *>(
    GetProcAddress(GetModuleHandleA("kernel32"), "CreatePseudoConsole")
);

static const auto closePseudoConsole = reinterpret_cast<decltype(ClosePseudoConsole) *>(
    GetProcAddress(GetModuleHandleA("kernel32"), "ClosePseudoConsole")
);

static const auto resizePseudoConsole = reinterpret_cast<decltype(ResizePseudoConsole) *>(
    GetProcAddress(GetModuleHandleA("kernel32"), "ResizePseudoConsole")
);
#else
constexpr auto NOTIFY_READER = 6;
constexpr auto NOTIFY_WRITER = 7;
#endif

#ifdef _WIN32
static std::string quote(const std::string &arg) {
    std::string result;

    const char *str = arg.c_str();
    const bool quote = strchr(str, ' ') != nullptr || strchr(str, '\t') != nullptr || *str == '\0';

    if (quote)
        result.push_back('\"');

    int bsCount = 0;

    for (const char *p = str; *p != '\0'; ++p) {
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
#endif

zero::os::process::Process::Process(ProcessImpl impl): mImpl(std::move(impl)) {
}

zero::os::process::ProcessImpl &zero::os::process::Process::impl() {
    return mImpl;
}

const zero::os::process::ProcessImpl &zero::os::process::Process::impl() const {
    return mImpl;
}

zero::os::process::ID zero::os::process::Process::pid() const {
#ifdef _WIN32
    return static_cast<ID>(mImpl.pid());
#else
    return mImpl.pid();
#endif
}

tl::expected<zero::os::process::ID, std::error_code> zero::os::process::Process::ppid() const {
#ifdef _WIN32
    return mImpl.ppid().transform([](const DWORD id) {
        return static_cast<ID>(id);
    });
#elif __APPLE__
    return mImpl.ppid();
#elif __linux__
    return mImpl.stat().transform(&procfs::process::Stat::ppid);
#endif
}

tl::expected<std::string, std::error_code> zero::os::process::Process::name() const {
#ifdef _WIN32
    return mImpl.name();
#else
    return mImpl.comm();
#endif
}

tl::expected<std::filesystem::path, std::error_code> zero::os::process::Process::cwd() const {
    return mImpl.cwd();
}

tl::expected<std::filesystem::path, std::error_code> zero::os::process::Process::exe() const {
    return mImpl.exe();
}

tl::expected<std::vector<std::string>, std::error_code> zero::os::process::Process::cmdline() const {
    return mImpl.cmdline();
}

tl::expected<std::map<std::string, std::string>, std::error_code> zero::os::process::Process::envs() const {
#ifdef __linux__
    return mImpl.environ();
#else
    return mImpl.envs();
#endif
}

tl::expected<zero::os::process::CPUTime, std::error_code> zero::os::process::Process::cpu() const {
#ifdef __linux__
    const auto result = unix::expected([&] {
        return sysconf(_SC_CLK_TCK);
    });
    EXPECT(result);

    const auto ticks = static_cast<double>(*result);
    const auto stat = mImpl.stat();
    EXPECT(stat);

    return CPUTime{
        static_cast<double>(stat->uTime) / ticks,
        static_cast<double>(stat->sTime) / ticks
    };
#else
    return mImpl.cpu().transform([](const auto &cpu) {
        return CPUTime{cpu.user, cpu.system};
    });
#endif
}

tl::expected<zero::os::process::MemoryStat, std::error_code> zero::os::process::Process::memory() const {
#ifdef __linux__
    const auto result = unix::expected([&] {
        return sysconf(_SC_PAGE_SIZE);
    });
    EXPECT(result);

    const auto pageSize = static_cast<std::uint64_t>(*result);
    const auto statM = mImpl.statM();
    EXPECT(statM);

    return MemoryStat{
        statM->resident * pageSize,
        statM->size * pageSize
    };
#else
    return mImpl.memory().transform([](const auto &memory) {
        return MemoryStat{memory.rss, memory.vms};
    });
#endif
}

tl::expected<zero::os::process::IOStat, std::error_code> zero::os::process::Process::io() const {
    return mImpl.io().transform([](const auto &io) {
        return IOStat{io.readBytes, io.writeBytes};
    });
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::os::process::Process::kill() {
#ifdef _WIN32
    return mImpl.terminate(EXIT_FAILURE);
#else
    EXPECT(unix::expected([&] {
        return ::kill(mImpl.pid(), SIGKILL);
    }));
    return {};
#endif
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::self() {
#ifdef _WIN32
    return nt::process::self().transform([](nt::process::Process &&process) {
        return Process{std::move(process)};
    });
#elif __APPLE__
    return darwin::process::self().transform([](darwin::process::Process &&process) {
        return Process{std::move(process)};
    });
#elif __linux__
    return procfs::process::self().transform([](procfs::process::Process &&process) {
        return Process{std::move(process)};
    });
#endif
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::open(const ID pid) {
#ifdef _WIN32
    return nt::process::open(static_cast<DWORD>(pid)).transform([](nt::process::Process &&process) {
        return Process{std::move(process)};
    });
#elif __APPLE__
    return darwin::process::open(pid).transform([](darwin::process::Process &&process) {
        return Process{std::move(process)};
    });
#elif __linux__
    return procfs::process::open(pid).transform([](procfs::process::Process &&process) {
        return Process{std::move(process)};
    });
#endif
}

tl::expected<std::list<zero::os::process::ID>, std::error_code> zero::os::process::all() {
#ifdef _WIN32
    return nt::process::all().transform([](const auto &ids) {
        const auto v = ids | ranges::views::transform([](const DWORD pid) {
            return static_cast<ID>(pid);
        });

        return std::list<ID>{v.begin(), v.end()};
    });
#elif __APPLE__
    return darwin::process::all();
#elif __linux__
    return procfs::process::all();
#endif
}

zero::os::process::ExitStatus::ExitStatus(const Status status) : mStatus(status) {
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
    : Process(std::move(process)), mStdio(stdio) {
}

zero::os::process::ChildProcess::ChildProcess(ChildProcess &&rhs) noexcept
    : Process(std::move(rhs)), mStdio(std::exchange(rhs.mStdio, {})) {
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
zero::os::process::PseudoConsole::PseudoConsole(const HPCON pc, const std::array<HANDLE, 4> &handles)
    : mPC(pc), mHandles(handles) {
}

zero::os::process::PseudoConsole::PseudoConsole(PseudoConsole &&rhs) noexcept
    : mPC(std::exchange(rhs.mPC, nullptr)), mHandles(std::exchange(rhs.mHandles, {})) {
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

tl::expected<zero::os::process::PseudoConsole, std::error_code>
zero::os::process::PseudoConsole::make(const short rows, const short columns) {
    if (!createPseudoConsole || !closePseudoConsole || !resizePseudoConsole)
        return tl::unexpected(Error::API_NOT_AVAILABLE);

    std::array<HANDLE, 4> handles = {};

    DEFER(
        for (const auto &handle: handles) {
            if (!handle)
                continue;

            CloseHandle(handle);
        }
    );

    EXPECT(nt::expected([&] {
        return CreatePipe(handles.data() + PTY_MASTER_READER, handles.data() + PTY_SLAVE_WRITER, nullptr, 0);
    }));

    EXPECT(nt::expected([&] {
        return CreatePipe(handles.data() + PTY_SLAVE_READER, handles.data() + PTY_MASTER_WRITER, nullptr, 0);
    }));

    HPCON hPC;

    if (const HRESULT result = createPseudoConsole(
        {columns, rows},
        handles[PTY_SLAVE_READER],
        handles[PTY_SLAVE_WRITER],
        0,
        &hPC
    ); result != S_OK)
        return tl::unexpected(static_cast<nt::ResultHandle>(result));

    return PseudoConsole{hPC, std::exchange(handles, {})};
}

void zero::os::process::PseudoConsole::close() {
    assert(mPC);
    closePseudoConsole(std::exchange(mPC, nullptr));
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    if (const HRESULT result = resizePseudoConsole(mPC, {columns, rows}); result != S_OK)
        return tl::unexpected(static_cast<nt::ResultHandle>(result));

    return {};
}

HANDLE &zero::os::process::PseudoConsole::input() {
    return mHandles[PTY_MASTER_WRITER];
}

HANDLE &zero::os::process::PseudoConsole::output() {
    return mHandles[PTY_MASTER_READER];
}
#else
zero::os::process::PseudoConsole::PseudoConsole(const int master, const int slave): mMaster(master), mSlave(slave) {
}

zero::os::process::PseudoConsole::PseudoConsole(PseudoConsole &&rhs) noexcept
    : mMaster(std::exchange(rhs.mMaster, -1)), mSlave(std::exchange(rhs.mSlave, -1)) {
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

tl::expected<zero::os::process::PseudoConsole, std::error_code>
zero::os::process::PseudoConsole::make(const short rows, const short columns) {
#if __ANDROID__ && __ANDROID_API__ < 23
    static const auto openpty = reinterpret_cast<int (*)(int *, int *, char *, const termios *, const winsize *)>(
        dlsym(RTLD_DEFAULT, "openpty")
    );

    if (!openpty)
        return tl::unexpected(Error::API_NOT_AVAILABLE);
#endif
    int master, slave;

    EXPECT(unix::expected([&] {
        return openpty(&master, &slave, nullptr, nullptr, nullptr);
    }));

    PseudoConsole pc = {master, slave};
    EXPECT(pc.resize(rows, columns));

    return pc;
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::os::process::PseudoConsole::resize(const short rows, const short columns) {
    winsize ws = {};

    ws.ws_row = rows;
    ws.ws_col = columns;

    EXPECT(unix::expected([&] {
        return ioctl(mMaster, TIOCSWINSZ, &ws);
    }));

    return {};
}

int &zero::os::process::PseudoConsole::fd() {
    return mMaster;
}
#endif

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<zero::os::process::ExitStatus, std::error_code> zero::os::process::ChildProcess::wait() {
#ifdef _WIN32
    const auto &impl = this->impl();

    EXPECT(impl.wait(std::nullopt));
    return impl.exitCode().transform([](const DWORD code) {
        return ExitStatus{code};
    });
#else
    int s;

    const pid_t pid = this->pid();
    const auto id = unix::ensure([&] {
        return waitpid(pid, &s, 0);
    });
    EXPECT(id);
    assert(*id == pid);

    return ExitStatus{s};
#endif
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<std::optional<zero::os::process::ExitStatus>, std::error_code> zero::os::process::ChildProcess::tryWait() {
#ifdef _WIN32
    using namespace std::chrono_literals;

    const auto &impl = this->impl();

    if (const auto result = impl.wait(0ms); !result) {
        if (result.error() == std::errc::timed_out)
            return std::nullopt;

        return tl::unexpected(result.error());
    }

    return impl.exitCode().transform([](const DWORD code) {
        return ExitStatus{code};
    });
#else
    const pid_t pid = this->pid();
    int s;

    const auto id = unix::expected([&] {
        return waitpid(pid, &s, WNOHANG);
    });
    EXPECT(id);

    if (*id == 0)
        return std::nullopt;

    return ExitStatus{s};
#endif
}

zero::os::process::Command::Command(std::filesystem::path path)
    : mInheritEnv(true), mPath(std::move(path)) {
}

tl::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::Command::spawn(const std::array<StdioType, 3> defaultTypes) const {
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr = {};

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = true;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE handles[6] = {};

    DEFER(
        for (const auto &handle: handles) {
            if (!handle)
                continue;

            CloseHandle(handle);
        }
    );

    constexpr std::array stdMapping = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    constexpr std::array indexMapping = {0, 3, 5};
    constexpr std::array<DWORD, 3> accessMapping = {GENERIC_READ, GENERIC_WRITE, GENERIC_WRITE};

    for (int i = 0; i < 3; ++i) {
        const auto type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == StdioType::INHERIT) {
            const auto handle = GetStdHandle(stdMapping[i]);

            if (!handle)
                continue;

            EXPECT(nt::expected([&] {
                return DuplicateHandle(
                    GetCurrentProcess(),
                    handle,
                    GetCurrentProcess(),
                    handles + indexMapping[i],
                    0,
                    true,
                    DUPLICATE_SAME_ACCESS
                );
            }));
            continue;
        }

        if (type == StdioType::PIPED) {
            EXPECT(nt::expected([&] {
                return CreatePipe(handles + i * 2, handles + i * 2 + 1, &saAttr, 0);
            }));
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
            return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

        handles[indexMapping[i]] = handle;
    }

    STARTUPINFOEXW siEx = {};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

    std::vector<HANDLE> inherits;

    if (const auto handle = handles[STDIN_READER]) {
        siEx.StartupInfo.hStdInput = handle;
        inherits.push_back(handle);
    }

    if (const auto handle = handles[STDOUT_WRITER]) {
        siEx.StartupInfo.hStdOutput = handle;
        inherits.push_back(handle);
    }

    if (const auto handle = handles[STDERR_WRITER]) {
        siEx.StartupInfo.hStdError = handle;
        inherits.push_back(handle);
    }

    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T size;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    EXPECT(nt::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
    }));

    EXPECT(nt::expected([&] {
        return UpdateProcThreadAttribute(
            siEx.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inherits.data(),
            inherits.size() * sizeof(HANDLE),
            nullptr,
            nullptr
        );
    }));

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        const auto ptr = GetEnvironmentStringsW();

        if (!ptr)
            return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

        DEFER(FreeEnvironmentStringsW(ptr));

        for (LPWCH env = ptr; *env != L'\0'; env += wcslen(env) + 1) {
            const auto str = strings::encode(env);
            EXPECT(str);

            auto tokens = strings::split(*str, "=", 1);

            if (tokens.size() != 2)
                continue;

            if (tokens[0].empty())
                continue;

            envs.emplace(std::move(tokens[0]), std::move(tokens[1]));
        }
    }

    for (const auto &[key, value]: this->envs()) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = strings::decode(to_string(fmt::join(
        envs | ranges::views::transform([](const auto &it) {
            std::string env = it.first + "=" + it.second;
            env.push_back('\0');
            return env;
        }),
        ""
    )));
    EXPECT(environment);

    std::vector arguments = {mPath.string()};
    ranges::copy(mArguments, ranges::back_inserter(arguments));

    auto cmd = strings::decode(to_string(
        fmt::join(arguments | ranges::views::transform(quote), " ")
    ));
    EXPECT(cmd);

    PROCESS_INFORMATION info;

    EXPECT(nt::expected([&] {
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

    if (handles[STDIN_WRITER])
        stdio[0] = std::exchange(handles[STDIN_WRITER], nullptr);

    if (handles[STDOUT_READER])
        stdio[1] = std::exchange(handles[STDOUT_READER], nullptr);

    if (handles[STDERR_READER])
        stdio[2] = std::exchange(handles[STDERR_READER], nullptr);

    return ChildProcess{Process{nt::process::Process{info.hProcess, info.dwProcessId}}, stdio};
#else
    int fds[8];
    ranges::fill(fds, -1);

    DEFER(
        for (const auto &fd: fds) {
            if (fd < 0)
                continue;

            close(fd);
        }
    );

    EXPECT(unix::expected([&] {
        return pipe(fds + NOTIFY_READER);
    }));

    EXPECT(unix::expected([&] {
        return fcntl(fds[NOTIFY_WRITER], F_SETFD, FD_CLOEXEC);
    }));

    constexpr std::array indexMapping = {0, 3, 5};
    constexpr std::array flagMapping = {O_RDONLY, O_WRONLY, O_WRONLY};

    for (int i = 0; i < 3; ++i) {
        const auto type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == StdioType::INHERIT)
            continue;

        if (type == StdioType::PIPED) {
            EXPECT(unix::expected([&] {
                return pipe(fds + i * 2);
            }));
            continue;
        }

        const auto fd = unix::expected([&] {
            return ::open("/dev/null", flagMapping[i]);
        });
        EXPECT(fd);

        fds[indexMapping[i]] = *fd;
    }

    assert(ranges::all_of(fds, [](const auto &fd) {return fd == -1 || fd > STDERR_FILENO;}));

    const std::string program = mPath.string();

    std::vector arguments = {program};
    ranges::copy(mArguments, ranges::back_inserter(arguments));

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        for (char **ptr = environ; *ptr != nullptr; ++ptr) {
            auto tokens = strings::split(*ptr, "=", 1);

            if (tokens.size() != 2)
                continue;

            envs.emplace(std::move(tokens[0]), std::move(tokens[1]));
        }
    }

    for (const auto &[key, value]: this->envs()) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    std::vector<std::string> environment;
    ranges::transform(envs, ranges::back_inserter(environment), [](const auto &it) {
        return fmt::format("{}={}", it.first, it.second);
    });

    const auto argv = std::make_unique<char *[]>(arguments.size() + 1);
    const auto envp = std::make_unique<char *[]>(environment.size() + 1);

    ranges::transform(arguments, argv.get(), [](auto &str) {
        return str.data();
    });

    ranges::transform(environment, envp.get(), [](auto &str) {
        return str.data();
    });

    const auto pid = unix::expected([] {
        return fork();
    });
    EXPECT(pid);

    if (*pid == 0) {
        if ((fds[STDIN_READER] > 0 && dup2(fds[STDIN_READER], STDIN_FILENO) < 0)
            || (fds[STDOUT_WRITER] > 0 && dup2(fds[STDOUT_WRITER], STDOUT_FILENO) < 0)
            || (fds[STDERR_WRITER] > 0 && dup2(fds[STDERR_WRITER], STDERR_FILENO) < 0)) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_WRITER], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }

        const auto max = static_cast<int>(sysconf(_SC_OPEN_MAX));

        for (int i = STDERR_FILENO + 1; i < max; ++i) {
            if (i == fds[NOTIFY_WRITER])
                continue;

            close(i);
        }

        if (mCurrentDirectory && chdir(mCurrentDirectory->string().c_str()) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_WRITER], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }

#ifdef __linux__
        if (execvpe(program.data(), argv.get(), envp.get()) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_WRITER], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }
#else
        environ = envp.get();

        if (execvp(program.data(), argv.get()) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[NOTIFY_WRITER], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }
#endif
    }

    close(std::exchange(fds[NOTIFY_WRITER], -1));

    int error;

    const auto n = unix::ensure([&] {
        return read(fds[NOTIFY_READER], &error, sizeof(int));
    });
    assert(n);

    if (*n != 0) {
        assert(n == sizeof(int));
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == pid);
        return tl::unexpected<std::error_code>(error, std::system_category());
    }

    auto process = open(*pid);

    if (!process) {
        kill(*pid, SIGKILL);
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == pid);
        return tl::unexpected(process.error());
    }

    std::array<std::optional<ChildProcess::StdioFile>, 3> stdio;

    if (fds[STDIN_WRITER] >= 0)
        stdio[0] = std::exchange(fds[STDIN_WRITER], -1);

    if (fds[STDOUT_READER] >= 0)
        stdio[1] = std::exchange(fds[STDOUT_READER], -1);

    if (fds[STDERR_READER] >= 0)
        stdio[2] = std::exchange(fds[STDERR_READER], -1);

    return ChildProcess{*std::move(process), stdio};
#endif
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

tl::expected<zero::os::process::ChildProcess, std::error_code> zero::os::process::Command::spawn() const {
    return spawn({StdioType::INHERIT, StdioType::INHERIT, StdioType::INHERIT});
}

tl::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::Command::spawn(PseudoConsole &pc) const {
#ifdef _WIN32
    assert(pc.mPC);
    assert(ranges::all_of(pc.mHandles, [](const auto &handle) {return handle != nullptr;}));

    STARTUPINFOEXW siEx = {};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);

    siEx.StartupInfo.hStdInput = nullptr;
    siEx.StartupInfo.hStdOutput = nullptr;
    siEx.StartupInfo.hStdError = nullptr;
    siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    SIZE_T size;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);

    const auto buffer = std::make_unique<std::byte[]>(size);
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    EXPECT(nt::expected([&] {
        return InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
    }));

    EXPECT(nt::expected([&] {
        return UpdateProcThreadAttribute(
            siEx.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            pc.mPC,
            sizeof(HPCON),
            nullptr,
            nullptr
        );
    }));

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        const auto ptr = GetEnvironmentStringsW();

        if (!ptr)
            return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

        DEFER(FreeEnvironmentStringsW(ptr));

        for (LPWCH env = ptr; *env != L'\0'; env += wcslen(env) + 1) {
            const auto str = strings::encode(env);
            EXPECT(str);

            auto tokens = strings::split(*str, "=", 1);

            if (tokens.size() != 2)
                continue;

            if (tokens[0].empty())
                continue;

            envs.emplace(std::move(tokens[0]), std::move(tokens[1]));
        }
    }

    for (const auto &[key, value]: this->envs()) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = strings::decode(to_string(fmt::join(
        envs | ranges::views::transform([](const auto &it) {
            std::string env = it.first + "=" + it.second;
            env.push_back('\0');
            return env;
        }),
        ""
    )));
    EXPECT(environment);

    std::vector arguments = {mPath.string()};
    ranges::copy(mArguments, ranges::back_inserter(arguments));

    auto cmd = strings::decode(to_string(
        fmt::join(arguments | ranges::views::transform(quote), " ")
    ));
    EXPECT(cmd);

    PROCESS_INFORMATION info;

    EXPECT(nt::expected([&] {
        return CreateProcessW(
            nullptr,
            cmd->data(),
            nullptr,
            nullptr,
            false,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            environment->data(),
            mCurrentDirectory ? mCurrentDirectory->wstring().c_str() : nullptr,
            &siEx.StartupInfo,
            &info
        );
    }));

    CloseHandle(info.hThread);
    CloseHandle(std::exchange(pc.mHandles[PTY_SLAVE_READER], nullptr));
    CloseHandle(std::exchange(pc.mHandles[PTY_SLAVE_WRITER], nullptr));

    return ChildProcess{Process{nt::process::Process{info.hProcess, info.dwProcessId}}, {}};
#else
    int fds[2];
    ranges::fill(fds, -1);

    DEFER(
        for (const auto &fd: fds) {
            if (fd < 0)
                continue;

            close(fd);
        }
    );

    EXPECT(unix::expected([&] {
        return pipe(fds);
    }));

    assert(pc.mMaster >= 0);
    assert(pc.mSlave > STDERR_FILENO);
    assert(ranges::all_of(fds, [](const auto &fd) {return fd > STDERR_FILENO;}));

    EXPECT(unix::expected([&] {
        return fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    }));

    const std::string program = mPath.string();

    std::vector arguments = {program};
    ranges::copy(mArguments, ranges::back_inserter(arguments));

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        for (char **ptr = environ; *ptr != nullptr; ++ptr) {
            auto tokens = strings::split(*ptr, "=", 1);

            if (tokens.size() != 2)
                continue;

            envs.emplace(std::move(tokens[0]), std::move(tokens[1]));
        }
    }

    for (const auto &[key, value]: this->envs()) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    std::vector<std::string> environment;
    ranges::transform(envs, ranges::back_inserter(environment), [](const auto &it) {
        return fmt::format("{}={}", it.first, it.second);
    });

    const auto argv = std::make_unique<char *[]>(arguments.size() + 1);
    const auto envp = std::make_unique<char *[]>(environment.size() + 1);

    ranges::transform(arguments, argv.get(), [](auto &str) {
        return str.data();
    });

    ranges::transform(environment, envp.get(), [](auto &str) {
        return str.data();
    });

    const auto pid = unix::expected([] {
        return fork();
    });
    EXPECT(pid);

    if (*pid == 0) {
        if (setsid() < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }

        if (ioctl(pc.mSlave, TIOCSCTTY, nullptr) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }

        if (dup2(pc.mSlave, STDIN_FILENO) < 0
            || dup2(pc.mSlave, STDOUT_FILENO) < 0
            || dup2(pc.mSlave, STDERR_FILENO) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }

        const auto max = static_cast<int>(sysconf(_SC_OPEN_MAX));

        for (int i = STDERR_FILENO + 1; i < max; ++i) {
            if (i == fds[1])
                continue;

            close(i);
        }

        if (mCurrentDirectory && chdir(mCurrentDirectory->string().c_str()) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }

#ifdef __linux__
        if (execvpe(program.data(), argv.get(), envp.get()) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }
#else
        environ = envp.get();

        if (execvp(program.data(), argv.get()) < 0) {
            const int error = errno;
            const auto n = unix::ensure([&] {
                return write(fds[1], &error, sizeof(int));
            });
            assert(n);
            std::abort();
        }
#endif
    }

    close(std::exchange(fds[1], -1));

    int error;

    const auto n = unix::ensure([&] {
        return read(fds[0], &error, sizeof(int));
    });
    assert(n);

    if (*n != 0) {
        assert(n == sizeof(int));
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == pid);
        return tl::unexpected<std::error_code>(error, std::system_category());
    }

    close(std::exchange(pc.mSlave, -1));

    auto process = open(*pid);

    if (!process) {
        kill(*pid, SIGKILL);
        const auto id = unix::ensure([&] {
            return waitpid(*pid, nullptr, 0);
        });
        assert(id);
        assert(*id == *pid);
        return tl::unexpected(process.error());
    }

    return ChildProcess{*std::move(process), {}};
#endif
}

tl::expected<zero::os::process::Output, std::error_code>
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

    const auto readAll = [](const auto &fd) -> tl::expected<std::vector<std::byte>, std::error_code> {
        if (!fd)
            return {};

        tl::expected<std::vector<std::byte>, std::error_code> result;

#ifdef _WIN32
        while (true) {
            DWORD n;
            std::byte buffer[1024];

            if (const auto res = nt::expected([&] {
                return ReadFile(*fd, buffer, sizeof(buffer), &n, nullptr);
            }); !res) {
                if (res.error() == std::errc::broken_pipe)
                    break;

                result = tl::unexpected(res.error());
                break;
            }

            assert(n > 0);
            std::copy_n(buffer, n, std::back_inserter(*result));
        }
#else
        while (true) {
            std::byte buffer[1024];
            const auto n = unix::ensure([&] {
                return read(*fd, buffer, sizeof(buffer));
            });
            EXPECT(n);

            if (*n == 0)
                break;

            std::copy_n(buffer, *n, std::back_inserter(*result));
        }
#endif

        return result;
    };

    auto future = std::async(readAll, child->stdError());
    auto out = readAll(child->stdOutput());
    EXPECT(out);

    auto err = future.get();
    EXPECT(err);

    const auto status = child->wait();
    EXPECT(status);

    return Output{
        *status,
        *std::move(out),
        *std::move(err)
    };
}
