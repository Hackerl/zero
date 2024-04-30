#include <zero/os/stat.h>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <map>
#include <regex>
#include <unistd.h>
#include <zero/expect.h>
#include <zero/strings/strings.h>
#include <zero/os/procfs/procfs.h>
#include <zero/filesystem/file.h>
#include <range/v3/algorithm.hpp>
#elif __APPLE__
#include <unistd.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <zero/os/darwin/error.h>
#endif

tl::expected<zero::os::stat::CPUTime, std::error_code> zero::os::stat::cpu() {
#ifdef _WIN32
    FILETIME idle, kernel, user;

    if (!GetSystemTimes(&idle, &kernel, &user))
        return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

    CPUTime time{
        static_cast<double>(user.dwHighDateTime) * 429.4967296 + static_cast<double>(user.dwLowDateTime) * 1e-7,
        static_cast<double>(kernel.dwHighDateTime) * 429.4967296 + static_cast<double>(kernel.dwLowDateTime) * 1e-7,
        static_cast<double>(idle.dwHighDateTime) * 429.4967296 + static_cast<double>(idle.dwLowDateTime) * 1e-7,
    };

    time.system = time.system - time.idle;
    return time;
#elif __linux__
    const auto content = filesystem::readString("/proc/stat");
    EXPECT(content);

    const auto lines = strings::split(strings::trim(*content), "\n");
    const auto it = ranges::find_if(
        lines,
        [](const auto &line) {
            if (line.length() < 4)
                return false;

            return std::regex_match(line.substr(0, 4), std::regex(R"(^cpu\s)"));
        }
    );

    if (it == lines.end())
        return tl::unexpected(procfs::Error::UNEXPECTED_DATA);

    const auto tokens = strings::split(*it);

    if (tokens.size() != 11)
        return tl::unexpected(procfs::Error::UNEXPECTED_DATA);

    const auto user = strings::toNumber<std::uint64_t>(tokens[1]);
    const auto system = strings::toNumber<std::uint64_t>(tokens[3]);
    const auto idle = strings::toNumber<std::uint64_t>(tokens[4]);

    EXPECT(user);
    EXPECT(system);
    EXPECT(idle);

    const long result = sysconf(_SC_CLK_TCK);

    if (result < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    const auto ticks = static_cast<double>(result);

    return CPUTime{
        static_cast<double>(*user) / ticks,
        static_cast<double>(*system) / ticks,
        static_cast<double>(*idle) / ticks
    };
#elif __APPLE__
    host_cpu_load_info_data_t data;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    if (const kern_return_t status = host_statistics(
        mach_host_self(),
        HOST_CPU_LOAD_INFO,
        reinterpret_cast<host_info_t>(&data),
        &count
    ); status != KERN_SUCCESS)
        return tl::unexpected(make_error_code(static_cast<darwin::Error>(status)));

    const long result = sysconf(_SC_CLK_TCK);

    if (result < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    const auto ticks = static_cast<double>(result);

    return CPUTime{
        data.cpu_ticks[CPU_STATE_USER] / ticks,
        data.cpu_ticks[CPU_STATE_SYSTEM] / ticks,
        data.cpu_ticks[CPU_STATE_IDLE] / ticks
    };
#endif
}

tl::expected<zero::os::stat::MemoryStat, std::error_code> zero::os::stat::memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status))
        return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

    return MemoryStat{
        status.ullTotalPhys,
        status.ullTotalPhys - status.ullAvailPhys,
        status.ullAvailPhys,
        status.ullAvailPhys,
        static_cast<double>(status.dwMemoryLoad)
    };
#elif __linux__
    const auto content = filesystem::readString("/proc/meminfo");
    EXPECT(content);

    std::map<std::string, std::uint64_t> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return tl::unexpected(procfs::Error::UNEXPECTED_DATA);

        const auto n = strings::toNumber<std::uint64_t>(strings::trim(tokens[1]));
        EXPECT(n);

        map.emplace(std::move(tokens[0]), *n * 1024);
    }

    if (map.find("MemTotal") == map.end() || map.find("MemFree") == map.end() ||
        map.find("Buffers") == map.end() || map.find("Cached") == map.end())
        return tl::unexpected(procfs::Error::UNEXPECTED_DATA);

    MemoryStat stat = {};

    stat.total = map["MemTotal"];
    stat.free = map["MemFree"];

    const auto buffers = map["Buffers"];
    const auto cached = map["Cached"];

    stat.used = stat.total - stat.free - buffers - cached;
    stat.usedPercent = static_cast<double>(stat.used) / static_cast<double>(stat.total) * 100.0;

    if (const auto it = map.find("Available"); it != map.end()) {
        stat.available = it->second;
        return stat;
    }

    stat.available = cached + stat.free;
    return stat;
#elif __APPLE__
    vm_statistics_data_t data;
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;

    if (const kern_return_t status = host_statistics(
        mach_host_self(),
        HOST_VM_INFO,
        reinterpret_cast<host_info_t>(&data),
        &count
    ); status != KERN_SUCCESS)
        return tl::unexpected(make_error_code(static_cast<darwin::Error>(status)));

    std::uint64_t total;
    std::size_t size = sizeof(total);

    if (int mib[2] = {CTL_HW, HW_MEMSIZE}; sysctl(mib, 2, &total, &size, nullptr, 0) != 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    MemoryStat stat = {};

    stat.total = total;
    stat.available = (data.inactive_count + data.free_count) * vm_kernel_page_size;
    stat.free = data.free_count * vm_kernel_page_size;
    stat.used = stat.total - stat.available;
    stat.usedPercent = static_cast<double>(stat.used) / static_cast<double>(stat.total) * 100.0;

    return stat;
#endif
}
