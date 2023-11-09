#include <zero/os/process.h>
#include <zero/try.h>

zero::os::process::Process::Process(ProcessImpl impl) : mImpl(std::move(impl)) {

}

zero::os::process::ID zero::os::process::Process::pid() const {
    return (ID) mImpl.pid();
}

tl::expected<zero::os::process::ID, std::error_code> zero::os::process::Process::ppid() const {
    auto ppid = TRY(mImpl.ppid());
    return (ID) *ppid;
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

tl::expected<zero::os::process::CPUStat, std::error_code> zero::os::process::Process::cpu() const {
    auto cpu = TRY(mImpl.cpu());
    return CPUStat{
            cpu->user,
            cpu->system
    };
}

tl::expected<zero::os::process::MemoryStat, std::error_code> zero::os::process::Process::memory() const {
    auto memory = TRY(mImpl.memory());
    return MemoryStat{
            memory->rss,
            memory->vms
    };
}

tl::expected<zero::os::process::IOStat, std::error_code> zero::os::process::Process::io() const {
    auto io = TRY(mImpl.io());
    return IOStat{
            io->readBytes,
            io->writeBytes
    };
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::self() {
#ifdef _WIN32
    auto impl = TRY(nt::process::self());
#elif __APPLE__
    auto impl = TRY(darwin::process::self());
#elif __linux__
    auto impl = TRY(procfs::self());
#endif

    return Process{std::move(*impl)};
}

tl::expected<zero::os::process::Process, std::error_code> zero::os::process::open(ID pid) {
#ifdef _WIN32
    auto impl = TRY(nt::process::open((DWORD) pid));
#elif __APPLE__
    auto impl = TRY(darwin::process::open(pid));
#elif __linux__
    auto impl = TRY(procfs::open(pid));
#endif

    return Process{std::move(*impl)};
}