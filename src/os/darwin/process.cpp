#include <zero/os/darwin/process.h>
#include <zero/os/darwin/error.h>
#include <zero/os/unix/error.h>
#include <zero/strings/strings.h>
#include <zero/expect.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <unistd.h>
#include <csignal>

zero::os::darwin::process::Process::Process(const pid_t pid) : mPID{pid} {
}

zero::os::darwin::process::Process::Process(Process &&rhs) noexcept : mPID{std::exchange(rhs.mPID, -1)} {
}

zero::os::darwin::process::Process &zero::os::darwin::process::Process::operator=(Process &&rhs) noexcept {
    mPID = std::exchange(rhs.mPID, -1);
    return *this;
}

std::expected<std::vector<char>, std::error_code> zero::os::darwin::process::Process::arguments() const {
    std::array mib{CTL_KERN, KERN_PROCARGS2, mPID};
    std::size_t size{10240};

    auto buffer = std::make_unique<char[]>(size);

    while (true) {
        const auto result = unix::expected([&] {
            return sysctl(mib.data(), mib.size(), buffer.get(), &size, nullptr, 0);
        });

        if (result)
            break;

        if (result.error() != std::errc::not_enough_memory)
            return std::unexpected(result.error());

        size *= 2;
        buffer = std::make_unique<char[]>(size);
    }

    return std::vector<char>{buffer.get(), buffer.get() + size};
}

pid_t zero::os::darwin::process::Process::pid() const {
    return mPID;
}

std::expected<pid_t, std::error_code> zero::os::darwin::process::Process::ppid() const {
    proc_bsdshortinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return static_cast<pid_t>(info.pbsi_ppid);
}

std::expected<std::string, std::error_code> zero::os::darwin::process::Process::comm() const {
    proc_bsdshortinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return info.pbsi_comm;
}

std::expected<std::string, std::error_code> zero::os::darwin::process::Process::name() const {
    std::array<char, 2 * MAXCOMLEN> buffer{};

    if (proc_name(mPID, buffer.data(), buffer.size()) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return buffer.data();
}

std::expected<std::filesystem::path, std::error_code> zero::os::darwin::process::Process::cwd() const {
    proc_vnodepathinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDVNODEPATHINFO, 0, &info, PROC_PIDVNODEPATHINFO_SIZE) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return info.pvi_cdir.vip_path;
}

std::expected<std::filesystem::path, std::error_code> zero::os::darwin::process::Process::exe() const {
    std::array<char, PROC_PIDPATHINFO_MAXSIZE> buffer{};

    if (proc_pidpath(mPID, buffer.data(), buffer.size()) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return buffer.data();
}

std::expected<std::vector<std::string>, std::error_code> zero::os::darwin::process::Process::cmdline() const {
    const auto arguments = this->arguments();
    EXPECT(arguments);

    if (arguments->size() < sizeof(int))
        return std::unexpected(Error::UNEXPECTED_DATA);

    const auto argc = *reinterpret_cast<const int *>(arguments->data());
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return std::unexpected(Error::UNEXPECTED_DATA);

    it = std::find_if(
        it,
        arguments->end(),
        [](const auto &c) {
            return c != '\0';
        }
    );

    if (it == arguments->end())
        return std::unexpected(Error::UNEXPECTED_DATA);

    auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return std::unexpected(Error::UNEXPECTED_DATA);

    tokens.pop_back();
    return tokens;
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::darwin::process::Process::envs() const {
    const auto arguments = this->arguments();
    EXPECT(arguments);

    if (arguments->size() < sizeof(int))
        return std::unexpected(Error::UNEXPECTED_DATA);

    const auto argc = *reinterpret_cast<const int *>(arguments->data());
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return std::unexpected(Error::UNEXPECTED_DATA);

    it = std::find_if(
        it,
        arguments->end(),
        [](const auto &c) {
            return c != '\0';
        }
    );

    if (it == arguments->end())
        return std::unexpected(Error::UNEXPECTED_DATA);

    const auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return std::unexpected(Error::UNEXPECTED_DATA);

    const auto &str = tokens[argc];

    if (str.empty())
        return {};

    std::size_t prev{0};
    std::map<std::string, std::string> envs;

    while (true) {
        if (str.length() <= prev)
            return std::unexpected(Error::UNEXPECTED_DATA);

        if (str[prev] == '\0')
            break;

        auto pos = str.find('\0', prev);

        if (pos == std::string::npos)
            return std::unexpected(Error::UNEXPECTED_DATA);

        const auto item = str.substr(prev, pos - prev);
        prev = pos + 1;
        pos = item.find('=');

        if (pos == std::string::npos)
            return std::unexpected(Error::UNEXPECTED_DATA);

        envs.emplace(item.substr(0, pos), item.substr(pos + 1));
    }

    return envs;
}

std::expected<zero::os::darwin::process::CPUTime, std::error_code> zero::os::darwin::process::Process::cpu() const {
    proc_taskinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    struct mach_timebase_info tb{};

    if (const auto status = mach_timebase_info(&tb); status != KERN_SUCCESS)
        return std::unexpected(make_error_code(static_cast<darwin::Error>(status)));

    const auto scale = static_cast<double>(tb.numer) / static_cast<double>(tb.denom);

    return CPUTime{
        static_cast<double>(info.pti_total_user) * scale / 1e9,
        static_cast<double>(info.pti_total_system) * scale / 1e9
    };
}

std::expected<zero::os::darwin::process::MemoryStat, std::error_code>
zero::os::darwin::process::Process::memory() const {
    proc_taskinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return MemoryStat{
        info.pti_resident_size,
        info.pti_virtual_size,
        static_cast<std::uint64_t>(info.pti_pageins)
    };
}

std::expected<zero::os::darwin::process::IOStat, std::error_code>
zero::os::darwin::process::Process::io() const {
    rusage_info_v2 info{};

    if (proc_pid_rusage(mPID, RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&info)) < 0)
        return std::unexpected(std::error_code(errno, std::system_category()));

    return IOStat{
        info.ri_diskio_bytesread,
        info.ri_diskio_byteswritten
    };
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::darwin::process::Process::kill(const int sig) {
    EXPECT(unix::expected([&] {
        return ::kill(mPID, sig);
    }));
    return {};
}

std::expected<zero::os::darwin::process::Process, std::error_code> zero::os::darwin::process::self() {
    return open(getpid());
}

std::expected<zero::os::darwin::process::Process, std::error_code> zero::os::darwin::process::open(const pid_t pid) {
    EXPECT(unix::expected([&] {
        return kill(pid, 0);
    }));
    return Process{pid};
}

std::expected<std::list<pid_t>, std::error_code> zero::os::darwin::process::all() {
    std::size_t size{4096};
    auto buffer = std::make_unique<pid_t[]>(size);

    while (true) {
        const auto n = proc_listallpids(buffer.get(), static_cast<int>(size * sizeof(pid_t)));

        if (n == -1)
            return std::unexpected(std::error_code(errno, std::system_category()));

        if (n < size)
            return std::list<pid_t>{buffer.get(), buffer.get() + n};

        size *= 2;
        buffer = std::make_unique<pid_t[]>(size);
    }
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::darwin::process::Process::Error)
