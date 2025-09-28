#include <zero/os/linux/process.h>
#include <zero/os/linux/procfs/procfs.h>
#include <zero/os/unix/error.h>
#include <zero/expect.h>
#include <unistd.h>
#include <csignal>
#include <pwd.h>

zero::os::linux::process::Process::Process(procfs::process::Process process) : mProcess{std::move(process)} {
}

zero::os::linux::process::Process::Process(Process &&rhs) noexcept : mProcess{std::move(rhs.mProcess)} {
}

zero::os::linux::process::Process &zero::os::linux::process::Process::operator=(Process &&rhs) noexcept {
    mProcess = std::move(rhs.mProcess);
    return *this;
}

pid_t zero::os::linux::process::Process::pid() const {
    return mProcess.pid();
}

std::expected<pid_t, std::error_code> zero::os::linux::process::Process::ppid() const {
    return mProcess.stat().transform([](const auto &stat) {
        return stat.ppid;
    });
}

std::expected<std::string, std::error_code> zero::os::linux::process::Process::comm() const {
    return mProcess.stat().transform([](const auto &stat) {
        return stat.comm;
    });
}

std::expected<std::filesystem::path, std::error_code> zero::os::linux::process::Process::cwd() const {
    return mProcess.cwd();
}

std::expected<std::filesystem::path, std::error_code> zero::os::linux::process::Process::exe() const {
    return mProcess.exe();
}

std::expected<std::vector<std::string>, std::error_code> zero::os::linux::process::Process::cmdline() const {
    return mProcess.cmdline();
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::linux::process::Process::envs() const {
    return mProcess.environ();
}

std::expected<std::chrono::system_clock::time_point, std::error_code>
zero::os::linux::process::Process::startTime() const {
    const auto ticks = unix::expected([] {
        return sysconf(_SC_CLK_TCK);
    }).transform([](const auto &value) {
        return static_cast<double>(value);
    });
    EXPECT(ticks);

    const auto stat = mProcess.stat();
    EXPECT(stat);

    const auto kernelStat = procfs::stat();
    EXPECT(kernelStat);

    return std::chrono::system_clock::from_time_t(
        static_cast<std::int64_t>(kernelStat->bootTime + static_cast<double>(stat->startTime) / *ticks)
    );
}

std::expected<zero::os::linux::process::CPUTime, std::error_code> zero::os::linux::process::Process::cpu() const {
    const auto ticks = unix::expected([] {
        return sysconf(_SC_CLK_TCK);
    }).transform([](const auto &value) {
        return static_cast<double>(value);
    });
    EXPECT(ticks);

    const auto stat = mProcess.stat();
    EXPECT(stat);

    return CPUTime{
        static_cast<double>(stat->userTime) / *ticks,
        static_cast<double>(stat->systemTime) / *ticks
    };
}

std::expected<zero::os::linux::process::MemoryStat, std::error_code> zero::os::linux::process::Process::memory() const {
    const auto pageSize = unix::expected([&] {
        return sysconf(_SC_PAGE_SIZE);
    }).transform([](const auto &value) {
        return static_cast<std::uint64_t>(value);
    });
    EXPECT(pageSize);

    const auto statM = mProcess.statM();
    EXPECT(statM);

    return MemoryStat{
        statM->residentSetSize * *pageSize,
        statM->totalSize * *pageSize
    };
}

std::expected<zero::os::linux::process::IOStat, std::error_code> zero::os::linux::process::Process::io() const {
    return mProcess.io();
}

std::expected<std::string, std::error_code> zero::os::linux::process::Process::user() const {
    const auto status = mProcess.status();
    EXPECT(status);

    const auto max = sysconf(_SC_GETPW_R_SIZE_MAX);

    auto size = static_cast<std::size_t>(max != -1 ? max : 1024);
    auto buffer = std::make_unique<char[]>(size);

    passwd pwd{};
    passwd *ptr{};

    while (true) {
        const auto n = getpwuid_r(status->uid[0], &pwd, buffer.get(), size, &ptr);

        if (n == 0) {
            if (!ptr)
                return std::unexpected{Error::NO_SUCH_USER};

            return pwd.pw_name;
        }

        if (n != ERANGE)
            return std::unexpected{std::error_code(n, std::system_category())};

        size *= 2;
        buffer = std::make_unique<char[]>(size);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::linux::process::Process::kill(const int sig) {
    EXPECT(unix::expected([&] {
        return ::kill(mProcess.pid(), sig);
    }));
    return {};
}

std::expected<zero::os::linux::process::Process, std::error_code> zero::os::linux::process::self() {
    return procfs::process::self().transform([](procfs::process::Process &&process) {
        return Process{std::move(process)};
    });
}

std::expected<zero::os::linux::process::Process, std::error_code> zero::os::linux::process::open(const pid_t pid) {
    return procfs::process::open(pid).transform([](procfs::process::Process &&process) {
        return Process{std::move(process)};
    });
}

std::expected<std::list<pid_t>, std::error_code> zero::os::linux::process::all() {
    return procfs::process::all();
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::linux::process::Process::Error)
