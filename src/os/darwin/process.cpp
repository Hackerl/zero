#include <zero/os/darwin/process.h>
#include <zero/strings/strings.h>
#include <zero/try.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <libproc.h>
#include <csignal>

const char *zero::os::darwin::process::ErrorCategory::name() const noexcept {
    return "zero::os::darwin::process";
}

std::string zero::os::darwin::process::ErrorCategory::message(int value) const {
    if (value == UNEXPECTED_DATA)
        return "unexpected data";

    return "unknown";
}

const std::error_category &zero::os::darwin::process::errorCategory() {
    static ErrorCategory instance;
    return instance;
}

std::error_code zero::os::darwin::process::make_error_code(Error e) {
    return {static_cast<int>(e), errorCategory()};
}


zero::os::darwin::process::Process::Process(pid_t pid) : mPID(pid) {

}

pid_t zero::os::darwin::process::Process::pid() const {
    return mPID;
}

tl::expected<pid_t, std::error_code> zero::os::darwin::process::Process::ppid() const {
    proc_bsdshortinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return info.pbsi_ppid;
}

tl::expected<std::string, std::error_code> zero::os::darwin::process::Process::comm() const {
    proc_bsdshortinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDT_SHORTBSDINFO, 0, &info, PROC_PIDT_SHORTBSDINFO_SIZE) <= 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return info.pbsi_comm;
}

tl::expected<std::string, std::error_code> zero::os::darwin::process::Process::name() const {
    char buffer[2 * MAXCOMLEN] = {};

    if (proc_name(mPID, buffer, sizeof(buffer)) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return buffer;
}

tl::expected<std::filesystem::path, std::error_code> zero::os::darwin::process::Process::cwd() const {
    proc_vnodepathinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDVNODEPATHINFO, 0, &info, PROC_PIDVNODEPATHINFO_SIZE) <= 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return info.pvi_cdir.vip_path;
}

tl::expected<std::filesystem::path, std::error_code> zero::os::darwin::process::Process::exe() const {
    char buffer[PROC_PIDPATHINFO_MAXSIZE] = {};

    if (proc_pidpath(mPID, buffer, sizeof(buffer)) <= 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return buffer;
}

tl::expected<std::vector<std::string>, std::error_code> zero::os::darwin::process::Process::cmdline() const {
    auto arguments = TRY(this->arguments());

    if (arguments->size() < sizeof(int))
        return tl::unexpected(Error::UNEXPECTED_DATA);

    int argc = *(int *) arguments->data();
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return tl::unexpected(Error::UNEXPECTED_DATA);

    it = std::find_if(
            it,
            arguments->end(),
            [](char c) {
                return c != '\0';
            }
    );

    if (it == arguments->end())
        return tl::unexpected(Error::UNEXPECTED_DATA);

    auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return tl::unexpected(Error::UNEXPECTED_DATA);

    tokens.pop_back();
    return tokens;
}

tl::expected<std::map<std::string, std::string>, std::error_code> zero::os::darwin::process::Process::environ() const {
    auto arguments = TRY(this->arguments());

    if (arguments->size() < sizeof(int))
        return tl::unexpected(Error::UNEXPECTED_DATA);

    int argc = *(int *) arguments->data();
    auto it = std::find(arguments->begin() + sizeof(int), arguments->end(), '\0');

    if (it == arguments->end())
        return tl::unexpected(Error::UNEXPECTED_DATA);

    it = std::find_if(
            it,
            arguments->end(),
            [](char c) {
                return c != '\0';
            }
    );

    if (it == arguments->end())
        return tl::unexpected(Error::UNEXPECTED_DATA);

    auto tokens = strings::split({it, arguments->end()}, {"\0", 1}, argc);

    if (tokens.size() != argc + 1)
        return tl::unexpected(Error::UNEXPECTED_DATA);

    tl::expected<std::map<std::string, std::string>, std::error_code> result;

    size_t prev = 0;
    auto &str = tokens[argc];

    while (true) {
        if (str.length() <= prev) {
            result = tl::unexpected<std::error_code>(Error::UNEXPECTED_DATA);
            break;
        }

        if (str[prev] == '\0')
            break;

        size_t pos = str.find('\0', prev);

        if (pos == std::string::npos) {
            result = tl::unexpected<std::error_code>(Error::UNEXPECTED_DATA);
            break;
        }

        auto item = str.substr(prev, pos - prev);
        prev = pos + 1;

        pos = item.find('=');

        if (pos == std::string::npos) {
            result = tl::unexpected<std::error_code>(Error::UNEXPECTED_DATA);
            break;
        }

        result->operator[](item.substr(0, pos)) = item.substr(pos + 1);
    }

    return result;
}

tl::expected<std::vector<char>, std::error_code> zero::os::darwin::process::Process::arguments() const {
    size_t size;
    int mib[3] = {CTL_KERN, KERN_PROCARGS2, mPID};

    if (sysctl(mib, 3, nullptr, &size, nullptr, 0) != 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    auto buffer = std::make_unique<char[]>(size);

    if (sysctl(mib, 3, buffer.get(), &size, nullptr, 0) != 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return std::vector<char>{buffer.get(), buffer.get() + size};
}

tl::expected<zero::os::darwin::process::CPUStat, std::error_code> zero::os::darwin::process::Process::cpu() const {
    proc_taskinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    struct mach_timebase_info tb = {};

    if (mach_timebase_info(&tb) != KERN_SUCCESS)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    double scale = double(tb.numer) / double(tb.denom);

    return CPUStat{
            .user = double(info.pti_total_user) * scale / 1e9,
            .system = double(info.pti_total_system) * scale / 1e9
    };
}

tl::expected<zero::os::darwin::process::MemoryStat, std::error_code>
zero::os::darwin::process::Process::memory() const {
    proc_taskinfo info = {};

    if (proc_pidinfo(mPID, PROC_PIDTASKINFO, 0, &info, PROC_PIDTASKINFO_SIZE) <= 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return MemoryStat{
            .rss = info.pti_resident_size,
            .vms = info.pti_virtual_size,
            .swap = (uint64_t) info.pti_pageins
    };
}

tl::expected<zero::os::darwin::process::Process, std::error_code> zero::os::darwin::process::open(pid_t pid) {
    if (kill(pid, 0) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return Process{pid};
}