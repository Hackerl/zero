#include <zero/os/stat.h>
#include <zero/expect.h>

#ifdef _WIN32
#include <windows.h>
#include <zero/os/windows/error.h>
#elif defined(__linux__)
#include <unistd.h>
#include <zero/os/unix/error.h>
#include <zero/os/linux/procfs/procfs.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <zero/os/unix/error.h>
#include <zero/os/macos/error.h>
#endif

tl::expected<zero::os::stat::CPUTime, std::error_code> zero::os::stat::cpu() {
#ifdef _WIN32
    FILETIME idle{}, kernel{}, user{};

    EXPECT(windows::expected([&] {
        return GetSystemTimes(&idle, &kernel, &user);
    }));

    CPUTime time{
        static_cast<double>(user.dwHighDateTime) * 429.4967296 + static_cast<double>(user.dwLowDateTime) * 1e-7,
        static_cast<double>(kernel.dwHighDateTime) * 429.4967296 + static_cast<double>(kernel.dwLowDateTime) * 1e-7,
        static_cast<double>(idle.dwHighDateTime) * 429.4967296 + static_cast<double>(idle.dwLowDateTime) * 1e-7,
    };

    time.system = time.system - time.idle;
    return time;
#elif defined(__linux__)
    const auto stat = linux::procfs::stat();
    EXPECT(stat);

    const auto ticks = unix::expected([] {
        return sysconf(_SC_CLK_TCK);
    }).transform([](const auto &result) {
        return static_cast<double>(result);
    });
    EXPECT(ticks);

    return CPUTime{
        static_cast<double>(stat->total.user) / *ticks,
        static_cast<double>(stat->total.system) / *ticks,
        static_cast<double>(stat->total.idle) / *ticks
    };
#elif defined(__APPLE__)
    host_cpu_load_info_data_t data{};
    mach_msg_type_number_t count{HOST_CPU_LOAD_INFO_COUNT};

    if (const auto status = host_statistics(
        mach_host_self(),
        HOST_CPU_LOAD_INFO,
        reinterpret_cast<host_info_t>(&data),
        &count
    ); status != KERN_SUCCESS)
        return tl::unexpected{static_cast<macos::Error>(status)};

    const auto ticks = unix::expected([] {
        return sysconf(_SC_CLK_TCK);
    }).transform([](const auto &result) {
        return static_cast<double>(result);
    });
    EXPECT(ticks);

    return CPUTime{
        data.cpu_ticks[CPU_STATE_USER] / *ticks,
        data.cpu_ticks[CPU_STATE_SYSTEM] / *ticks,
        data.cpu_ticks[CPU_STATE_IDLE] / *ticks
    };
#endif
}

tl::expected<zero::os::stat::MemoryStat, std::error_code> zero::os::stat::memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    EXPECT(windows::expected([&] {
        return GlobalMemoryStatusEx(&status);
    }));

    return MemoryStat{
        status.ullTotalPhys,
        status.ullTotalPhys - status.ullAvailPhys,
        status.ullAvailPhys,
        status.ullAvailPhys,
        static_cast<double>(status.dwMemoryLoad)
    };
#elif defined(__linux__)
    const auto memory = linux::procfs::memory();
    EXPECT(memory);

    MemoryStat stat;

    stat.total = memory->memoryTotal * 1024;
    stat.free = memory->memoryFree * 1024;

    const auto buffers = memory->buffers * 1024;
    const auto cached = memory->cached * 1024;

    stat.used = stat.total - stat.free - buffers - cached;
    stat.usedPercent = static_cast<double>(stat.used) / static_cast<double>(stat.total) * 100.0;

    if (const auto &available = memory->memoryAvailable) {
        stat.available = *available * 1024;
        return stat;
    }

    stat.available = cached + stat.free;
    return stat;
#elif defined(__APPLE__)
    vm_statistics_data_t data{};
    mach_msg_type_number_t count{HOST_VM_INFO_COUNT};

    if (const auto status = host_statistics(
        mach_host_self(),
        HOST_VM_INFO,
        reinterpret_cast<host_info_t>(&data),
        &count
    ); status != KERN_SUCCESS)
        return tl::unexpected{static_cast<macos::Error>(status)};

    std::uint64_t total{};
    auto size = sizeof(total);

    EXPECT(unix::expected([&] {
        std::array mib{CTL_HW, HW_MEMSIZE};
        return sysctl(mib.data(), mib.size(), &total, &size, nullptr, 0);
    }));

    MemoryStat stat;

    stat.total = total;
    stat.available = (data.inactive_count + data.free_count) * vm_kernel_page_size;
    stat.free = data.free_count * vm_kernel_page_size;
    stat.used = stat.total - stat.available;
    stat.usedPercent = static_cast<double>(stat.used) / static_cast<double>(stat.total) * 100.0;

    return stat;
#endif
}
