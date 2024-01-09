#include <zero/os/process.h>
#include <zero/expect.h>

#ifdef _WIN32
#include <ranges>
#endif

zero::os::process::Process::Process(ProcessImpl impl) : mImpl(std::move(impl)) {
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
    return mImpl.ppid().transform([](const auto pid) {
        return static_cast<DWORD>(pid);
    });
#else
    return mImpl.ppid();
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

tl::expected<std::map<std::string, std::string>, std::error_code> zero::os::process::Process::env() const {
    return mImpl.env();
}

tl::expected<zero::os::process::CPUTime, std::error_code> zero::os::process::Process::cpu() const {
    return mImpl.cpu().transform([](const auto &cpu) {
        return CPUTime{
            cpu.user,
            cpu.system
        };
    });
}

tl::expected<zero::os::process::MemoryStat, std::error_code> zero::os::process::Process::memory() const {
    return mImpl.memory().transform([](const auto &memory) {
        return MemoryStat{
            memory.rss,
            memory.vms
        };
    });
}

tl::expected<zero::os::process::IOStat, std::error_code> zero::os::process::Process::io() const {
    return mImpl.io().transform([](const auto &io) {
        return IOStat{
            io.readBytes,
            io.writeBytes
        };
    });
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::self() {
#ifdef _WIN32
    auto impl = nt::process::self();
#elif __APPLE__
    auto impl = darwin::process::self();
#elif __linux__
    auto impl = procfs::process::self();
#endif

    EXPECT(impl);
    return Process{std::move(*impl)};
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::open(ID pid) {
#ifdef _WIN32
    auto impl = nt::process::open(static_cast<DWORD>(pid));
#elif __APPLE__
    auto impl = darwin::process::open(pid);
#elif __linux__
    auto impl = procfs::process::open(pid);
#endif

    EXPECT(impl);
    return Process{std::move(*impl)};
}

tl::expected<std::list<zero::os::process::ID>, std::error_code> zero::os::process::all() {
#ifdef _WIN32
    return nt::process::all().transform([](const auto &ids) {
        const auto v = ids | std::views::transform([](const auto pid) {
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
