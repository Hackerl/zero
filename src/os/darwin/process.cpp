#include <zero/os/darwin/process.h>
#include <zero/os/darwin/error.h>
#include <zero/strings/strings.h>
#include <zero/expect.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <unistd.h>
#include <csignal>

const char *zero::os::darwin::process::ErrorCategory::name() const noexcept {
    return "zero::os::darwin::process";
}

std::string zero::os::darwin::process::ErrorCategory::message(const int value) const {
    if (value == UNEXPECTED_DATA)
        return "unexpected data";

    return "unknown";
}

const std::error_category &zero::os::darwin::process::errorCategory() {
    static ErrorCategory instance;
    return instance;
}

std::error_code zero::os::darwin::process::make_error_code(const Error e) {
    return {static_cast<int>(e), errorCategory()};
}


zero::os::darwin::process::Process::Process(const pid_t pid) : mPID(pid) {
}

zero::os::darwin::process::Process::Process(Process &&rhs) noexcept : mPID(std::exchange(rhs.mPID, -1)) {
}

zero::os::darwin::process::Process &zero::os::darwin::process::Process::operator=(Process &&rhs) noexcept {
    mPID = std::exchange(rhs.mPID, -1);
    return *this;
}

tl::expected<std::vector<char>, std::error_code> zero::os::darwin::process::Process::arguments() const {
    std::size_t size;
    int mib[3] = {CTL_KERN, KERN_PROCARGS2, mPID};

    if (sysctl(mib, 3, nullptr, &size, nullptr, 0) != 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    const auto buffer = std::make_unique<char[]>(size);

    if (sysctl(mib, 3, buffer.get(), &size, nullptr, 0) != 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return std::vector<char>{buffer.get(), buffer.get() + size};
}

pid_t zero::os::darwin::process::Process::pid() const {
    return mPID;
}

tl::expected<pid_t, std::error_code> zero::os::darwin::process::Process::ppid() const {
    proc_bsdshortinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return static_cast<pid_t>(info.pbsi_ppid);
}

tl::expected<std::string, std::error_code> zero::os::darwin::process::Process::comm() const {
    proc_bsdshortinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return info.pbsi_comm;
}

tl::expected<std::string, std::error_code> zero::os::darwin::process::Process::name() const {
    char buffer[2 * MAXCOMLEN] = {};

    if (proc_name(mPID, buffer, sizeof(buffer)) < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return buffer;
}

tl::expected<std::filesystem::path, std::error_code> zero::os::darwin::process::Process::cwd() const {
    proc_vnodepathinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDVNODEPATHINFO, 0, &info, PROC_PIDVNODEPATHINFO_SIZE) <= 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return info.pvi_cdir.vip_path;
}

tl::expected<std::filesystem::path, std::error_code> zero::os::darwin::process::Process::exe() const {
    char buffer[PROC_PIDPATHINFO_MAXSIZE] = {};

    if (proc_pidpath(mPID, buffer, sizeof(buffer)) <= 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return buffer;
}

tl::expected<std::vector<std::string>, std::error_code> zero::os::darwin::process::Process::cmdline() const {
    const auto arguments = this->arguments();
    EXPECT(arguments);

    if (arguments->size() < sizeof(int))
        return tl::unexpected(UNEXPECTED_DATA);

    const auto argc = *reinterpret_cast<const int *>(arguments->data());
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return tl::unexpected(UNEXPECTED_DATA);

    it = std::find_if(
        it,
        arguments->end(),
        [](const char c) {
            return c != '\0';
        }
    );

    if (it == arguments->end())
        return tl::unexpected(UNEXPECTED_DATA);

    auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return tl::unexpected(UNEXPECTED_DATA);

    tokens.pop_back();
    return tokens;
}

tl::expected<std::map<std::string, std::string>, std::error_code> zero::os::darwin::process::Process::envs() const {
    const auto arguments = this->arguments();
    EXPECT(arguments);

    if (arguments->size() < sizeof(int))
        return tl::unexpected(UNEXPECTED_DATA);

    const int argc = *reinterpret_cast<const int *>(arguments->data());
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return tl::unexpected(UNEXPECTED_DATA);

    it = std::find_if(
        it,
        arguments->end(),
        [](const char c) {
            return c != '\0';
        }
    );

    if (it == arguments->end())
        return tl::unexpected(UNEXPECTED_DATA);

    const auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return tl::unexpected(UNEXPECTED_DATA);

    const auto &str = tokens[argc];

    if (str.empty())
        return {};

    std::size_t prev = 0;
    tl::expected<std::map<std::string, std::string>, std::error_code> result;

    while (true) {
        if (str.length() <= prev) {
            result = tl::unexpected<std::error_code>(UNEXPECTED_DATA);
            break;
        }

        if (str[prev] == '\0')
            break;

        std::size_t pos = str.find('\0', prev);

        if (pos == std::string::npos) {
            result = tl::unexpected<std::error_code>(UNEXPECTED_DATA);
            break;
        }

        const auto item = str.substr(prev, pos - prev);
        prev = pos + 1;
        pos = item.find('=');

        if (pos == std::string::npos) {
            result = tl::unexpected<std::error_code>(UNEXPECTED_DATA);
            break;
        }

        result->operator[](item.substr(0, pos)) = item.substr(pos + 1);
    }

    return result;
}

tl::expected<zero::os::darwin::process::CPUTime, std::error_code> zero::os::darwin::process::Process::cpu() const {
    proc_taskinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    struct mach_timebase_info tb = {};

    if (const kern_return_t status = mach_timebase_info(&tb); status != KERN_SUCCESS)
        return tl::unexpected(make_error_code(static_cast<darwin::Error>(status)));

    const auto scale = static_cast<double>(tb.numer) / static_cast<double>(tb.denom);

    return CPUTime{
        static_cast<double>(info.pti_total_user) * scale / 1e9,
        static_cast<double>(info.pti_total_system) * scale / 1e9
    };
}

tl::expected<zero::os::darwin::process::MemoryStat, std::error_code>
zero::os::darwin::process::Process::memory() const {
    proc_taskinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return MemoryStat{
        info.pti_resident_size,
        info.pti_virtual_size,
        static_cast<std::uint64_t>(info.pti_pageins)
    };
}

tl::expected<zero::os::darwin::process::IOStat, std::error_code>
zero::os::darwin::process::Process::io() const {
    rusage_info_v2 info = {};

    if (proc_pid_rusage(mPID, RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&info)) < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return IOStat{
        info.ri_diskio_bytesread,
        info.ri_diskio_byteswritten
    };
}

// ReSharper disable once CppMemberFunctionMayBeConst
tl::expected<void, std::error_code> zero::os::darwin::process::Process::kill(const int sig) {
    if (::kill(mPID, sig) < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return {};
}

tl::expected<zero::os::darwin::process::Process, std::error_code> zero::os::darwin::process::self() {
    return open(getpid());
}

tl::expected<zero::os::darwin::process::Process, std::error_code> zero::os::darwin::process::open(const pid_t pid) {
    if (kill(pid, 0) < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    return Process{pid};
}

tl::expected<std::list<pid_t>, std::error_code> zero::os::darwin::process::all() {
    std::size_t size = 4096;
    auto buffer = std::make_unique<pid_t[]>(size);

    tl::expected<std::list<pid_t>, std::error_code> result;

    while (true) {
        const int n = proc_listallpids(buffer.get(), static_cast<int>(size * sizeof(pid_t)));

        if (n == -1) {
            result = tl::unexpected<std::error_code>(errno, std::system_category());
            break;
        }

        if (n == size) {
            size *= 2;
            buffer = std::make_unique<pid_t[]>(size);
        }

        result->assign(buffer.get(), buffer.get() + n);
        break;
    }

    return result;
}
