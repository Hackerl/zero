#include <zero/os/process.h>
#include <zero/strings/strings.h>
#include <zero/expect.h>
#include <zero/defer.h>
#include <fmt/ranges.h>
#include <cassert>
#include <future>

#ifdef _WIN32
#include <ranges>
#else
#include <csignal>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#ifdef __APPLE__
extern char **environ;
#endif

#ifdef __linux__
zero::os::process::Process::Process(procfs::process::Process impl): mImpl(std::move(impl)) {
}

zero::os::procfs::process::Process &zero::os::process::Process::impl() {
    return mImpl;
}

const zero::os::procfs::process::Process &zero::os::process::Process::impl() const {
    return mImpl;
}

pid_t zero::os::process::Process::pid() const {
    return mImpl.pid();
}

tl::expected<pid_t, std::error_code> zero::os::process::Process::ppid() const {
    return mImpl.stat().transform([](const auto stat) {
        return stat.ppid;
    });
}

tl::expected<std::string, std::error_code> zero::os::process::Process::name() const {
    return mImpl.comm();
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
    return mImpl.environ();
}

tl::expected<zero::os::process::CPUTime, std::error_code> zero::os::process::Process::cpu() const {
    const long result = sysconf(_SC_CLK_TCK);

    if (result < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    const auto ticks = static_cast<double>(result);
    const auto stat = mImpl.stat();
    EXPECT(stat);

    CPUTime time = {
        static_cast<double>(stat->uTime) / ticks,
        static_cast<double>(stat->sTime) / ticks
    };

    if (stat->delayAcctBlkIOTicks)
        time.ioWait = static_cast<double>(*stat->delayAcctBlkIOTicks) / ticks;

    return time;
}

tl::expected<zero::os::process::MemoryStat, std::error_code> zero::os::process::Process::memory() const {
    const long result = sysconf(_SC_PAGE_SIZE);

    if (result < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    const auto pageSize = static_cast<std::uint64_t>(result);
    const auto statM = mImpl.statM();
    EXPECT(statM);

    return MemoryStat{
        statM->resident * pageSize,
        statM->size * pageSize
    };
}

tl::expected<zero::os::process::IOStat, std::error_code> zero::os::process::Process::io() const {
    return mImpl.io();
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::os::process::Process::kill(const int sig) {
    if (::kill(mImpl.pid(), sig) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return {};
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::self() {
    return procfs::process::self().transform([](procfs::process::Process &&process) {
        return Process{std::move(process)};
    });
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::open(pid_t pid) {
    return procfs::process::open(pid).transform([](procfs::process::Process &&process) {
        return Process{std::move(process)};
    });
}
#endif

zero::os::process::ChildProcess::ChildProcess(Process process, const std::array<std::optional<Stdio>, 3> &stdio)
    : Process(std::move(process)), mStdio(stdio) {
}

zero::os::process::ChildProcess::ChildProcess(ChildProcess &&rhs) noexcept
    : Process(std::move(rhs)), mStdio(std::exchange(rhs.mStdio, {})) {
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

std::optional<zero::os::process::ChildProcess::Stdio> &zero::os::process::ChildProcess::stdInput() {
    return mStdio[0];
}

std::optional<zero::os::process::ChildProcess::Stdio> &zero::os::process::ChildProcess::stdOutput() {
    return mStdio[1];
}

std::optional<zero::os::process::ChildProcess::Stdio> &zero::os::process::ChildProcess::stdError() {
    return mStdio[2];
}

#ifndef _WIN32
tl::expected<int, std::error_code> zero::os::process::ChildProcess::wait() const {
    int s;

    if (const pid_t pid = this->pid(); waitpid(pid, &s, 0) != pid)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return s;
}

tl::expected<int, std::error_code> zero::os::process::ChildProcess::tryWait() const {
    const pid_t pid = this->pid();
    int s;

    if (const int result = waitpid(pid, &s, WNOHANG); result != pid) {
        if (result == 0)
            return tl::unexpected(make_error_code(std::errc::operation_would_block));

        return tl::unexpected(std::error_code(errno, std::system_category()));
    }

    return s;
}
#endif

zero::os::process::Command::Command(std::filesystem::path path)
    : mInheritEnv(true), mPath(std::move(path)) {
}

tl::expected<zero::os::process::ChildProcess, std::error_code>
zero::os::process::Command::spawn(std::array<StdioType, 3> defaultTypes) const {
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr = {};

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = true;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE handles[6] = {};

    DEFER(
        for (const auto &handle : handles) {
            if (!handle)
                continue;

            CloseHandle(handle);
        }
    );

    for (int i = 0; i < 3; i++) {
        const bool input = i == 0;
        const StdioType type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == INHERIT)
            continue;

        if (type == PIPED) {
            if (!CreatePipe(handles + i * 2, handles + i * 2 + 1, &saAttr, 0))
                return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

            continue;
        }

        const auto handle = CreateFileA(
            R"(\\.\NUL)",
            input ? GENERIC_READ : GENERIC_WRITE,
            0,
            &saAttr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE)
            return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

        handles[i * 2 + (input ? 0 : 1)] = handle;
    }

    if ((handles[1] && !SetHandleInformation(handles[1], HANDLE_FLAG_INHERIT, 0))
        || (handles[2] && !SetHandleInformation(handles[2], HANDLE_FLAG_INHERIT, 0))
        || (handles[4] && !SetHandleInformation(handles[4], HANDLE_FLAG_INHERIT, 0)))
        return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    const bool redirect = handles[0] || handles[3] || handles[5];
    STARTUPINFO si = {};

    si.cb = sizeof(si);

    if (redirect) {
        si.hStdInput = handles[0] ? handles[0] : GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = handles[3] ? handles[3] : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = handles[5] ? handles[5] : GetStdHandle(STD_ERROR_HANDLE);
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        const auto ptr = GetEnvironmentStringsW();

        if (!ptr)
            return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

        DEFER(FreeEnvironmentStringsW(ptr));

        for (LPWCH env = ptr; *env != L'\0'; env += wcslen(env) + 1) {
            const auto str = strings::encode(env);
            EXPECT(str);

            auto tokens = strings::split(*str, "=", 1);

            if (tokens.size() != 2)
                continue;

            if (tokens[0].empty())
                continue;

            envs[std::move(tokens[0])] = std::move(tokens[1]);
        }
    }

    for (const auto &[key, value]: this->envs()) {
        if (!value) {
            envs.erase(key);
            continue;
        }

        envs[key] = *value;
    }

    auto environment = to_string(fmt::join(
        envs | std::views::transform([](const auto &it) {
            std::string env = it.first + "=" + it.second;
            env.push_back('\0');
            return env;
        }),
        ""
    ));

    std::vector arguments{mPath.string()};
    std::ranges::copy(mArguments, std::back_inserter(arguments));

    auto cmd = to_string(fmt::join(
        arguments | std::views::transform([](const auto &arg) {
            return fmt::format("{:?}", arg);
        }),
        " "
    ));

    PROCESS_INFORMATION info;

    if (!CreateProcessA(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        redirect,
        0,
        environment.data(),
        mCurrentDirectory ? mCurrentDirectory->string().c_str() : nullptr,
        &si,
        &info
    ))
        return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    CloseHandle(info.hThread);

    std::array<std::optional<HANDLE>, 3> stdio;

    if (handles[1])
        stdio[0] = std::exchange(handles[1], nullptr);

    if (handles[2])
        stdio[1] = std::exchange(handles[2], nullptr);

    if (handles[4])
        stdio[2] = std::exchange(handles[4], nullptr);

    return ChildProcess{Process{info.hProcess, info.dwProcessId}, stdio};
#else
    int fds[8];
    std::ranges::fill(fds, -1);

    DEFER(
        for (const auto &fd : fds) {
            if (fd < 0)
                continue;

            close(fd);
        }
    );

    if (pipe(fds + 6) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    if (fcntl(fds[7], F_SETFD, FD_CLOEXEC) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    for (int i = 0; i < 3; i++) {
        const bool input = i == 0;
        const StdioType type = mStdioTypes[i].value_or(defaultTypes[i]);

        if (type == INHERIT)
            continue;

        if (type == PIPED) {
            if (pipe(fds + i * 2) < 0)
                return tl::unexpected(std::error_code(errno, std::system_category()));

            continue;
        }

        const int fd = ::open("/dev/null", input ? O_RDONLY : O_WRONLY);

        if (fd < 0)
            return tl::unexpected(std::error_code(errno, std::system_category()));

        fds[i * 2 + (input ? 0 : 1)] = fd;
    }

    const std::string program = mPath.string();

    std::vector arguments{program};
    std::ranges::copy(mArguments, std::back_inserter(arguments));

    std::map<std::string, std::string> envs;

    if (mInheritEnv) {
        for (char **ptr = environ; *ptr != nullptr; ptr++) {
            auto tokens = strings::split(*ptr, "=", 1);

            if (tokens.size() != 2)
                continue;

            envs[std::move(tokens[0])] = std::move(tokens[1]);
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
    std::ranges::transform(envs, std::back_inserter(environment), [](const auto &it) {
        return fmt::format("{}={}", it.first, it.second);
    });

    const auto argv = std::make_unique<char *[]>(arguments.size() + 1);
    const auto envp = std::make_unique<char *[]>(environment.size() + 1);

    std::ranges::transform(arguments, argv.get(), [](auto &str) {
        return str.data();
    });

    std::ranges::transform(environment, envp.get(), [](auto &str) {
        return str.data();
    });

    const pid_t pid = fork();

    if (pid < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    if (pid == 0) {
        if ((fds[0] > 0 && dup2(fds[0], STDIN_FILENO) < 0)
            || (fds[3] > 0 && dup2(fds[3], STDOUT_FILENO) < 0)
            || (fds[5] > 0 && dup2(fds[5], STDERR_FILENO) < 0)) {
            const int error = errno;
            write(fds[7], &error, sizeof(int));
            std::abort();
        }

        const auto max = static_cast<int>(sysconf(_SC_OPEN_MAX));

        for (int i = STDERR_FILENO + 1; i < max; i++) {
            if (i == fds[7])
                continue;

            close(i);
        }

        if (mCurrentDirectory && chdir(mCurrentDirectory->string().c_str()) < 0) {
            const int error = errno;
            write(fds[7], &error, sizeof(int));
            std::abort();
        }

#ifdef __linux__
        if (execvpe(program.data(), argv.get(), envp.get()) < 0) {
            const int error = errno;
            write(fds[7], &error, sizeof(int));
            std::abort();
        }
#else
        environ = envp.get();

        if (execvp(program.data(), argv.get()) < 0) {
            const int error = errno;
            write(fds[7], &error, sizeof(int));
            std::abort();
        }
#endif
    }

    close(std::exchange(fds[7], -1));

    int error;

    if (const int n = read(fds[6], &error, sizeof(int)); n != 0) {
        assert(n == sizeof(int));
        waitpid(pid, nullptr, 0);
        return tl::unexpected(std::error_code(error, std::system_category()));
    }

    auto process = open(pid);

    if (!process) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        return tl::unexpected(process.error());
    }

    std::array<std::optional<int>, 3> stdio;

    if (fds[1] > 0)
        stdio[0] = std::exchange(fds[1], -1);

    if (fds[2] > 0)
        stdio[1] = std::exchange(fds[2], -1);

    if (fds[4] > 0)
        stdio[2] = std::exchange(fds[4], -1);

    return ChildProcess{std::move(*process), stdio};
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

zero::os::process::Command &zero::os::process::Command::envs(const std::map<std::string, std::string> &envs) {
    for (const auto &[key, value]: envs)
        mEnviron[key] = value;

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

zero::os::process::Command &zero::os::process::Command::stdIntput(const StdioType type) {
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
    return spawn({INHERIT, INHERIT, INHERIT});
}

tl::expected<zero::os::process::Output, std::error_code>
zero::os::process::Command::output() const {
    auto child = spawn({NUL, PIPED, PIPED});
    EXPECT(child);

    const auto readAll = [](const auto &fd) -> tl::expected<std::vector<std::byte>, std::error_code> {
        if (!fd)
            return tl::unexpected(make_error_code(std::errc::bad_file_descriptor));

        tl::expected<std::vector<std::byte>, std::error_code> result;

#ifdef _WIN32
        while (true) {
            DWORD n;
            std::byte buffer[1024];

            if (!ReadFile(*fd, buffer, sizeof(buffer), &n, nullptr)) {
                const DWORD error = GetLastError();

                if (error == ERROR_BROKEN_PIPE)
                    break;

                result = tl::unexpected(std::error_code(static_cast<int>(error), std::system_category()));
                break;
            }

            assert(n > 0);
            std::copy_n(buffer, n, std::back_inserter(*result));
        }
#else
        while (true) {
            std::byte buffer[1024];
            const ssize_t n = read(*fd, buffer, sizeof(buffer));

            if (n == 0)
                break;

            if (n < 0) {
                result = tl::unexpected(std::error_code(errno, std::system_category()));
                break;
            }

            std::copy_n(buffer, n, std::back_inserter(*result));
        }
#endif

        return result;
    };

    auto future = std::async(readAll, child->stdError());
    auto out = readAll(child->stdOutput());
    EXPECT(out);

    auto err = future.get();
    EXPECT(err);

#ifdef _WIN32
    const auto result = child->wait();
    EXPECT(result);

    const auto status = child->exitCode();
    EXPECT(status);
#else
    const auto status = child->wait();
    EXPECT(status);
#endif

    return Output{
        *status,
        std::move(*out),
        std::move(*err)
    };
}
