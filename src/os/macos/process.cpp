#include <zero/os/macos/process.h>
#include <zero/os/macos/error.h>
#include <zero/os/unix/error.h>
#include <zero/strings/strings.h>
#include <zero/expect.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <unistd.h>
#include <csignal>
#include <pwd.h>

zero::os::macos::process::Process::Process(const pid_t pid) : mPID{pid} {
}

zero::os::macos::process::Process::Process(Process &&rhs) noexcept : mPID{std::exchange(rhs.mPID, -1)} {
}

zero::os::macos::process::Process &zero::os::macos::process::Process::operator=(Process &&rhs) noexcept {
    mPID = std::exchange(rhs.mPID, -1);
    return *this;
}

std::expected<std::vector<char>, std::error_code> zero::os::macos::process::Process::arguments() const {
    std::array mib{CTL_KERN, KERN_PROCARGS2, mPID};
    std::size_t size{10240};

    auto buffer = std::make_unique<char[]>(size);

    while (true) {
        const auto result = unix::expected([&] {
            return sysctl(mib.data(), mib.size(), buffer.get(), &size, nullptr, 0);
        });

        if (result)
            break;

        if (const auto &error = result.error(); error != std::errc::not_enough_memory)
            return std::unexpected{error};

        size *= 2;
        buffer = std::make_unique<char[]>(size);
    }

    return std::vector<char>{buffer.get(), buffer.get() + size};
}

pid_t zero::os::macos::process::Process::pid() const {
    return mPID;
}

std::expected<pid_t, std::error_code> zero::os::macos::process::Process::ppid() const {
    proc_bsdshortinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return static_cast<pid_t>(info.pbsi_ppid);
}

std::expected<std::string, std::error_code> zero::os::macos::process::Process::comm() const {
    proc_bsdshortinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return info.pbsi_comm;
}

std::expected<std::string, std::error_code> zero::os::macos::process::Process::name() const {
    std::array<char, 2 * MAXCOMLEN> buffer{};

    if (proc_name(mPID, buffer.data(), buffer.size()) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return buffer.data();
}

std::expected<std::filesystem::path, std::error_code> zero::os::macos::process::Process::cwd() const {
    proc_vnodepathinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDVNODEPATHINFO, 0, &info, PROC_PIDVNODEPATHINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return info.pvi_cdir.vip_path;
}

std::expected<std::filesystem::path, std::error_code> zero::os::macos::process::Process::exe() const {
    std::array<char, PROC_PIDPATHINFO_MAXSIZE> buffer{};

    if (proc_pidpath(mPID, buffer.data(), buffer.size()) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return buffer.data();
}

std::expected<std::vector<std::string>, std::error_code> zero::os::macos::process::Process::cmdline() const {
    const auto arguments = this->arguments();
    Z_EXPECT(arguments);

    if (arguments->size() < sizeof(int))
        return std::unexpected{Error::UnexpectedData};

    const auto argc = *reinterpret_cast<const int *>(arguments->data());
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return std::unexpected{Error::UnexpectedData};

    it = std::find_if(
        it,
        arguments->end(),
        [](const auto &c) {
            return c != '\0';
        }
    );

    if (it == arguments->end())
        return std::unexpected{Error::UnexpectedData};

    auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return std::unexpected{Error::UnexpectedData};

    tokens.pop_back();
    return tokens;
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::macos::process::Process::envs() const {
    const auto arguments = this->arguments();
    Z_EXPECT(arguments);

    if (arguments->size() < sizeof(int))
        return std::unexpected{Error::UnexpectedData};

    const auto argc = *reinterpret_cast<const int *>(arguments->data());
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return std::unexpected{Error::UnexpectedData};

    it = std::find_if(
        it,
        arguments->end(),
        [](const auto &c) {
            return c != '\0';
        }
    );

    if (it == arguments->end())
        return std::unexpected{Error::UnexpectedData};

    const auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return std::unexpected{Error::UnexpectedData};

    const auto &str = tokens[argc];

    if (str.empty())
        return {};

    std::size_t prev{0};
    std::map<std::string, std::string> envs;

    while (true) {
        if (str.length() <= prev)
            return std::unexpected{Error::UnexpectedData};

        if (str[prev] == '\0')
            break;

        auto pos = str.find('\0', prev);

        if (pos == std::string::npos)
            return std::unexpected{Error::UnexpectedData};

        const auto item = str.substr(prev, pos - prev);
        prev = pos + 1;
        pos = item.find('=');

        // There is a case where only key does not contain `=`, which is ignored for now.
        if (pos == std::string::npos)
            continue;

        envs.emplace(item.substr(0, pos), item.substr(pos + 1));
    }

    return envs;
}

std::expected<std::chrono::system_clock::time_point, std::error_code>
zero::os::macos::process::Process::startTime() const {
    proc_bsdinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDTBSDINFO, 0, &info, PROC_PIDTBSDINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(info.pbi_start_tvsec));
}

std::expected<zero::os::macos::process::CPUTime, std::error_code> zero::os::macos::process::Process::cpu() const {
    proc_taskinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    struct mach_timebase_info tb{};

    if (const auto status = mach_timebase_info(&tb); status != KERN_SUCCESS)
        return std::unexpected{static_cast<macos::Error>(status)};

    const auto scale = static_cast<double>(tb.numer) / static_cast<double>(tb.denom);

    return CPUTime{
        static_cast<double>(info.pti_total_user) * scale / 1e9,
        static_cast<double>(info.pti_total_system) * scale / 1e9
    };
}

std::expected<zero::os::macos::process::MemoryStat, std::error_code>
zero::os::macos::process::Process::memory() const {
    proc_taskinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return MemoryStat{
        info.pti_resident_size,
        info.pti_virtual_size,
        static_cast<std::uint64_t>(info.pti_pageins)
    };
}

std::expected<zero::os::macos::process::IOStat, std::error_code>
zero::os::macos::process::Process::io() const {
    rusage_info_v2 info{};

    if (proc_pid_rusage(mPID, RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&info)) < 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    return IOStat{
        info.ri_diskio_bytesread,
        info.ri_diskio_byteswritten
    };
}

std::expected<std::string, std::error_code> zero::os::macos::process::Process::user() const {
    proc_bsdinfo info{};

    if (proc_pidinfo(mPID, PROC_PIDTBSDINFO, 0, &info, PROC_PIDTBSDINFO_SIZE) <= 0)
        return std::unexpected{std::error_code{errno, std::system_category()}};

    const auto max = sysconf(_SC_GETPW_R_SIZE_MAX);

    auto size = static_cast<std::size_t>(max != -1 ? max : 1024);
    auto buffer = std::make_unique<char[]>(size);

    passwd pwd{};
    passwd *ptr{};

    while (true) {
        const auto n = getpwuid_r(info.pbi_uid, &pwd, buffer.get(), size, &ptr);

        if (n == 0) {
            if (!ptr)
                return std::unexpected{Error::NoSuchUser};

            return pwd.pw_name;
        }

        if (n != ERANGE)
            return std::unexpected{std::error_code(n, std::system_category())};

        size *= 2;
        buffer = std::make_unique<char[]>(size);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::macos::process::Process::kill(const int sig) {
    Z_EXPECT(unix::expected([&] {
        return ::kill(mPID, sig);
    }));
    return {};
}

std::expected<zero::os::macos::process::Process, std::error_code> zero::os::macos::process::self() {
    return open(getpid());
}

std::expected<zero::os::macos::process::Process, std::error_code> zero::os::macos::process::open(const pid_t pid) {
    // When `SIP` is enabled, calling `kill(0, pid)` on certain system processes will result in an `Operation not permitted` error.
    Z_EXPECT(unix::expected([&] {
        return kill(pid, 0);
    }).transform([](const auto &) {
    }).or_else([&](const auto &ec) -> std::expected<void, std::error_code> {
        if (ec != std::errc::operation_not_permitted)
            return std::unexpected{ec};

        proc_bsdshortinfo info{};

        if (proc_pidinfo(pid, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
            return std::unexpected{std::error_code{errno, std::system_category()}};

        return {};
    }));

    return Process{pid};
}

std::expected<std::list<pid_t>, std::error_code> zero::os::macos::process::all() {
    std::size_t size{4096};
    auto buffer = std::make_unique<pid_t[]>(size);

    while (true) {
        const auto n = proc_listallpids(buffer.get(), static_cast<int>(size * sizeof(pid_t)));

        if (n == -1)
            return std::unexpected{std::error_code{errno, std::system_category()}};

        if (n < size)
            return std::list<pid_t>{buffer.get(), buffer.get() + n};

        size *= 2;
        buffer = std::make_unique<pid_t[]>(size);
    }
}

Z_DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::macos::process::Process::Error)
