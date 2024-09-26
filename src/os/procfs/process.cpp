#include <zero/os/procfs/process.h>
#include <zero/os/procfs/procfs.h>
#include <zero/detail/type_traits.h>
#include <zero/strings/strings.h>
#include <zero/os/unix/error.h>
#include <zero/defer.h>
#include <zero/expect.h>
#include <climits>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ranges>
#include <algorithm>

constexpr auto STAT_BASIC_FIELDS = 37;
constexpr auto MAPPING_BASIC_FIELDS = 5;
constexpr auto MAPPING_PERMISSIONS_LENGTH = 4;

zero::os::procfs::process::Process::Process(const int fd, const pid_t pid) : mFD{fd}, mPID{pid} {
}

zero::os::procfs::process::Process::Process(Process &&rhs) noexcept
    : mFD{std::exchange(rhs.mFD, -1)}, mPID{std::exchange(rhs.mPID, -1)} {
}

zero::os::procfs::process::Process &zero::os::procfs::process::Process::operator=(Process &&rhs) noexcept {
    mFD = std::exchange(rhs.mFD, -1);
    mPID = std::exchange(rhs.mPID, -1);
    return *this;
}

zero::os::procfs::process::Process::~Process() {
    if (mFD < 0)
        return;

    close(mFD);
}

std::expected<std::string, std::error_code> zero::os::procfs::process::Process::readFile(const char *filename) const {
    const auto fd = unix::expected([&] {
        return openat(mFD, filename, O_RDONLY);
    });
    EXPECT(fd);
    DEFER(close(*fd));

    std::expected<std::string, std::error_code> result;

    while (true) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        const auto n = unix::ensure([&] {
            return read(*fd, buffer.data(), buffer.size());
        });

        if (!n) {
            result = std::unexpected(n.error());
            break;
        }

        if (*n == 0)
            break;

        result->append(buffer.data(), *n);
    }

    return result;
}

pid_t zero::os::procfs::process::Process::pid() const {
    return mPID;
}

std::expected<std::filesystem::path, std::error_code> zero::os::procfs::process::Process::exe() const {
    std::array<char, PATH_MAX + 1> buffer{};

    EXPECT(unix::expected([&] {
        return readlinkat(mFD, "exe", buffer.data(), PATH_MAX);
    }));

    return buffer.data();
}

std::expected<std::filesystem::path, std::error_code> zero::os::procfs::process::Process::cwd() const {
    std::array<char, PATH_MAX + 1> buffer{};

    EXPECT(unix::expected([&] {
        return readlinkat(mFD, "cwd", buffer.data(), PATH_MAX);
    }));

    return buffer.data();
}

std::expected<std::string, std::error_code> zero::os::procfs::process::Process::comm() const {
    auto content = readFile("comm");
    EXPECT(content);

    if (content->size() < 2)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    content->pop_back();
    return *std::move(content);
}

std::expected<std::vector<std::string>, std::error_code> zero::os::procfs::process::Process::cmdline() const {
    auto content = readFile("cmdline");
    EXPECT(content);

    if (content->empty())
        return std::unexpected(Error::MAYBE_ZOMBIE_PROCESS);

    content->pop_back();
    auto tokens = strings::split(*content, {"\0", 1});

    if (tokens.empty())
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    return tokens;
}

std::expected<std::map<std::string, std::string>, std::error_code> zero::os::procfs::process::Process::environ() const {
    auto content = readFile("environ");
    EXPECT(content);

    if (content->empty())
        return {};

    content->pop_back();
    std::expected<std::map<std::string, std::string>, std::error_code> result;

    for (const auto &env: strings::split(*content, {"\0", 1})) {
        auto tokens = strings::split(env, "=", 1);

        if (tokens.size() != 2) {
            result = std::unexpected<std::error_code>(procfs::Error::UNEXPECTED_DATA);
            break;
        }

        result->emplace(std::move(tokens[0]), std::move(tokens[1]));
    }

    return result;
}

std::expected<zero::os::procfs::process::Stat, std::error_code> zero::os::procfs::process::Process::stat() const {
    const auto content = readFile("stat");
    EXPECT(content);

    const auto start = content->find('(');
    const auto end = content->rfind(')');

    if (start == std::string::npos || end == std::string::npos)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    Stat stat{};

    stat.pid = strings::toNumber<pid_t>(content->substr(0, start - 1)).value_or(-1);
    stat.comm = content->substr(start + 1, end - start - 1);

    const auto tokens = strings::split(content->substr(end + 2), " ");

    if (tokens.size() < STAT_BASIC_FIELDS - 2)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    auto it = tokens.begin();

    stat.state = it++->at(0);

    const auto set = [&]<typename T>(T &var) -> std::expected<void, std::error_code> {
        if constexpr (detail::is_specialization<T, std::optional>) {
            if (it == tokens.end())
                return {};

            const auto value = strings::toNumber<typename T::value_type>(*it++);
            EXPECT(value);
            var = *value;
        }
        else {
            const auto value = strings::toNumber<T>(*it++);
            EXPECT(value);
            var = *value;
        }

        return {};
    };

    EXPECT(set(stat.ppid));
    EXPECT(set(stat.pgrp));
    EXPECT(set(stat.session));
    EXPECT(set(stat.tty));
    EXPECT(set(stat.tpgid));
    EXPECT(set(stat.flags));
    EXPECT(set(stat.minFlt));
    EXPECT(set(stat.cMinFlt));
    EXPECT(set(stat.majFlt));
    EXPECT(set(stat.cMajFlt));
    EXPECT(set(stat.uTime));
    EXPECT(set(stat.sTime));
    EXPECT(set(stat.cuTime));
    EXPECT(set(stat.csTime));
    EXPECT(set(stat.priority));
    EXPECT(set(stat.nice));
    EXPECT(set(stat.numThreads));
    EXPECT(set(stat.intervalValue));
    EXPECT(set(stat.startTime));
    EXPECT(set(stat.vSize));
    EXPECT(set(stat.rss));
    EXPECT(set(stat.rssLimit));
    EXPECT(set(stat.startCode));
    EXPECT(set(stat.endCode));
    EXPECT(set(stat.startStack));
    EXPECT(set(stat.esp));
    EXPECT(set(stat.eip));
    EXPECT(set(stat.signal));
    EXPECT(set(stat.blocked));
    EXPECT(set(stat.sigIgnore));
    EXPECT(set(stat.sigCatch));
    EXPECT(set(stat.wChan));
    EXPECT(set(stat.nSwap));
    EXPECT(set(stat.cnSwap));
    EXPECT(set(stat.exitSignal));
    EXPECT(set(stat.processor));
    EXPECT(set(stat.rtPriority));
    EXPECT(set(stat.policy));
    EXPECT(set(stat.delayAcctBlkIOTicks));
    EXPECT(set(stat.guestTime));
    EXPECT(set(stat.cGuestTime));
    EXPECT(set(stat.startData));
    EXPECT(set(stat.endData));
    EXPECT(set(stat.startBrk));
    EXPECT(set(stat.argStart));
    EXPECT(set(stat.argEnd));
    EXPECT(set(stat.envStart));
    EXPECT(set(stat.envEnd));
    EXPECT(set(stat.exitCode));

    return stat;
}

std::expected<zero::os::procfs::process::StatM, std::error_code> zero::os::procfs::process::Process::statM() const {
    const auto content = readFile("statm");
    EXPECT(content);

    const auto tokens = strings::split(*content, " ");

    if (tokens.size() != 7)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    const auto size = strings::toNumber<std::uint64_t>(tokens[0]);
    const auto resident = strings::toNumber<std::uint64_t>(tokens[1]);
    const auto shared = strings::toNumber<std::uint64_t>(tokens[2]);
    const auto text = strings::toNumber<std::uint64_t>(tokens[3]);
    const auto library = strings::toNumber<std::uint64_t>(tokens[4]);
    const auto data = strings::toNumber<std::uint64_t>(tokens[5]);
    const auto dirty = strings::toNumber<std::uint64_t>(tokens[6]);

    EXPECT(size);
    EXPECT(resident);
    EXPECT(shared);
    EXPECT(text);
    EXPECT(library);
    EXPECT(data);
    EXPECT(dirty);

    return StatM{
        *size,
        *resident,
        *shared,
        *text,
        *library,
        *data,
        *dirty
    };
}

template<typename T>
std::optional<T>
statusIntegerField(const std::map<std::string, std::string> &map, const char *key, const int base = 10) {
    const auto it = map.find(key);

    if (it == map.end())
        return std::nullopt;

    return *zero::strings::toNumber<T>(it->second, base);
}

std::expected<zero::os::procfs::process::Status, std::error_code> zero::os::procfs::process::Process::status() const {
    const auto content = readFile("status");
    EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected(procfs::Error::UNEXPECTED_DATA);

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    Status status{};

    status.name = map["Name"];
    status.umask = statusIntegerField<mode_t>(map, "Umask", 8);
    status.state = map["State"];
    status.tgid = *strings::toNumber<pid_t>(map["Tgid"]);
    status.ngid = statusIntegerField<pid_t>(map, "Ngid");
    status.pid = *strings::toNumber<pid_t>(map["Pid"]);
    status.ppid = *strings::toNumber<pid_t>(map["PPid"]);
    status.tracerPID = *strings::toNumber<pid_t>(map["TracerPid"]);

    auto tokens = strings::split(map["Uid"]);

    if (tokens.size() != 4)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    for (std::size_t i{0}; i < 4; ++i)
        status.uid[i] = *strings::toNumber<uid_t>(tokens[i]);

    tokens = strings::split(map["Gid"]);

    if (tokens.size() != 4)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    for (std::size_t i{0}; i < 4; ++i)
        status.gid[i] = *strings::toNumber<pid_t>(tokens[i]);

    status.fdSize = *strings::toNumber<int>(map["FDSize"]);

    for (const auto &token: strings::split(map["Groups"]))
        status.groups.emplace_back(*strings::toNumber<pid_t>(token));

    status.nstgid = statusIntegerField<pid_t>(map, "NStgid");
    status.nspid = statusIntegerField<pid_t>(map, "NSpid");
    status.nspgid = statusIntegerField<pid_t>(map, "NSpgid");
    status.nssid = statusIntegerField<int>(map, "NSsid");
    status.vmPeak = statusIntegerField<unsigned long>(map, "VmPeak");
    status.vmSize = statusIntegerField<unsigned long>(map, "VmSize");
    status.vmLck = statusIntegerField<unsigned long>(map, "VmLck");
    status.vmPin = statusIntegerField<unsigned long>(map, "VmPin");
    status.vmHWM = statusIntegerField<unsigned long>(map, "VmHWM");
    status.vmRSS = statusIntegerField<unsigned long>(map, "VmRSS");
    status.rssAnon = statusIntegerField<unsigned long>(map, "RssAnon");
    status.rssFile = statusIntegerField<unsigned long>(map, "RssFile");
    status.rssShMem = statusIntegerField<unsigned long>(map, "RssShmem");
    status.vmData = statusIntegerField<unsigned long>(map, "VmData");
    status.vmStk = statusIntegerField<unsigned long>(map, "VmStk");
    status.vmExe = statusIntegerField<unsigned long>(map, "VmExe");
    status.vmLib = statusIntegerField<unsigned long>(map, "VmLib");
    status.vmPTE = statusIntegerField<unsigned long>(map, "VmPTE");
    status.vmPMD = statusIntegerField<unsigned long>(map, "VmPMD");
    status.vmSwap = statusIntegerField<unsigned long>(map, "VmSwap");
    status.hugeTLBPages = statusIntegerField<unsigned long>(map, "HugetlbPages");
    status.threads = *statusIntegerField<int>(map, "Threads");

    tokens = strings::split(map["SigQ"], "/", 1);

    if (tokens.size() != 2)
        return std::unexpected(procfs::Error::UNEXPECTED_DATA);

    status.sigQ[0] = *strings::toNumber<int>(tokens[0]);
    status.sigQ[1] = *strings::toNumber<int>(tokens[1]);

    status.sigPnd = *strings::toNumber<unsigned long>(map["SigPnd"], 16);
    status.shdPnd = *strings::toNumber<unsigned long>(map["ShdPnd"], 16);
    status.sigBlk = *strings::toNumber<unsigned long>(map["SigBlk"], 16);
    status.sigIgn = *strings::toNumber<unsigned long>(map["SigIgn"], 16);
    status.sigCgt = *strings::toNumber<unsigned long>(map["SigCgt"], 16);
    status.capInh = *strings::toNumber<unsigned long>(map["CapInh"], 16);
    status.capPrm = *strings::toNumber<unsigned long>(map["CapPrm"], 16);
    status.capEff = *strings::toNumber<unsigned long>(map["CapEff"], 16);
    status.capBnd = statusIntegerField<unsigned long>(map, "CapBnd", 16);
    status.capAmb = statusIntegerField<unsigned long>(map, "CapAmb", 16);
    status.noNewPrivileges = statusIntegerField<unsigned long>(map, "NoNewPrivs");
    status.seccomp = statusIntegerField<int>(map, "Seccomp");

    auto it = map.find("Speculation_Store_Bypass");

    if (it != map.end())
        status.speculationStoreBypass = it->second;

    it = map.find("Cpus_allowed");

    if (it != map.end()) {
        status.cpusAllowed.emplace();

        for (const auto &token: strings::split(it->second, ",")) {
            status.cpusAllowed->emplace_back(*strings::toNumber<unsigned int>(token, 16));
        }
    }

    it = map.find("Cpus_allowed_list");

    if (it != map.end()) {
        status.cpusAllowedList.emplace();

        for (const auto &token: strings::split(it->second, ",")) {
            if (!token.contains('-')) {
                const auto n = *strings::toNumber<unsigned int>(token);
                status.cpusAllowedList->emplace_back(n, n);
                break;
            }

            tokens = strings::split(token, "-", 1);

            if (tokens.size() != 2)
                continue;

            status.cpusAllowedList->emplace_back(
                *strings::toNumber<unsigned int>(tokens[0]),
                *strings::toNumber<unsigned int>(tokens[1])
            );
        }
    }

    it = map.find("Mems_allowed");

    if (it != map.end()) {
        status.memoryNodesAllowed.emplace();

        for (const auto &token: strings::split(it->second, ",")) {
            status.memoryNodesAllowed->emplace_back(*strings::toNumber<unsigned int>(token, 16));
        }
    }

    it = map.find("Mems_allowed_list");

    if (it != map.end()) {
        status.memoryNodesAllowedList.emplace();

        for (const auto &token: strings::split(it->second, ",")) {
            if (!token.contains('-')) {
                const auto n = *strings::toNumber<unsigned int>(token);
                status.memoryNodesAllowedList->emplace_back(n, n);
                break;
            }

            tokens = strings::split(token, "-", 1);

            if (tokens.size() != 2)
                continue;

            status.memoryNodesAllowedList->emplace_back(
                *strings::toNumber<unsigned int>(tokens[0]),
                *strings::toNumber<unsigned int>(tokens[1])
            );
        }
    }

    status.voluntaryContextSwitches = statusIntegerField<int>(map, "voluntary_ctxt_switches");
    status.nonVoluntaryContextSwitches = statusIntegerField<int>(map, "nonvoluntary_ctxt_switches");

    it = map.find("CoreDumping");

    if (it != map.end())
        status.coreDumping = it->second == "1";

    it = map.find("THP_enabled");

    if (it != map.end())
        status.thpEnabled = it->second == "1";

    return status;
}

std::expected<std::list<pid_t>, std::error_code> zero::os::procfs::process::Process::tasks() const {
    using namespace std::string_view_literals;

    const auto fd = unix::expected([&] {
        return openat(mFD, "task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    });
    EXPECT(fd);

    const auto dir = fdopendir(*fd);

    if (!dir) {
        close(*fd);
        return std::unexpected(std::error_code(errno, std::system_category()));
    }

    DEFER(closedir(dir));
    std::expected<std::list<pid_t>, std::error_code> result;

    while (true) {
        const auto *e = readdir(dir);

        if (!e)
            break;

        if (e->d_name == "."sv || e->d_name == ".."sv)
            continue;

        const auto tid = strings::toNumber<pid_t>(e->d_name);

        if (!tid) {
            result = std::unexpected(tid.error());
            break;
        }

        result->push_back(*tid);
    }

    return result;
}

std::expected<std::list<zero::os::procfs::process::MemoryMapping>, std::error_code>
zero::os::procfs::process::Process::maps() const {
    const auto content = readFile("maps");
    EXPECT(content);

    if (content->empty())
        return std::unexpected(Error::MAYBE_ZOMBIE_PROCESS);

    std::expected<std::list<MemoryMapping>, std::error_code> result;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        const auto fields = strings::split(line);

        if (fields.size() < MAPPING_BASIC_FIELDS) {
            result = std::unexpected<std::error_code>(procfs::Error::UNEXPECTED_DATA);
            break;
        }

        const auto tokens = strings::split(fields[0], "-", 1);

        if (tokens.size() != 2) {
            result = std::unexpected<std::error_code>(procfs::Error::UNEXPECTED_DATA);
            break;
        }

        const auto start = strings::toNumber<std::uintptr_t>(tokens[0], 16);

        if (!start) {
            result = std::unexpected(start.error());
            break;
        }

        const auto end = strings::toNumber<std::uintptr_t>(tokens[1], 16);

        if (!end) {
            result = std::unexpected(end.error());
            break;
        }

        const auto offset = strings::toNumber<off_t>(fields[2], 16);

        if (!offset) {
            result = std::unexpected(offset.error());
            break;
        }

        const auto inode = strings::toNumber<ino_t>(fields[4]);

        if (!inode) {
            result = std::unexpected(inode.error());
            break;
        }

        MemoryMapping memoryMapping{};

        memoryMapping.start = *start;
        memoryMapping.end = *end;
        memoryMapping.offset = *offset;
        memoryMapping.inode = *inode;
        memoryMapping.device = fields[3];

        if (fields.size() > MAPPING_BASIC_FIELDS)
            memoryMapping.pathname = fields[5];

        const auto &permissions = fields[1];

        if (permissions.length() < MAPPING_PERMISSIONS_LENGTH) {
            result = std::unexpected<std::error_code>(procfs::Error::UNEXPECTED_DATA);
            break;
        }

        if (permissions.at(0) == 'r')
            memoryMapping.permissions |= READ;

        if (permissions.at(1) == 'w')
            memoryMapping.permissions |= WRITE;

        if (permissions.at(2) == 'x')
            memoryMapping.permissions |= EXECUTE;

        if (permissions.at(3) == 's')
            memoryMapping.permissions |= SHARED;

        if (permissions.at(3) == 'p')
            memoryMapping.permissions |= PRIVATE;

        result->push_back(std::move(memoryMapping));
    }

    return result;
}

std::expected<zero::os::procfs::process::IOStat, std::error_code> zero::os::procfs::process::Process::io() const {
    const auto content = readFile("io");
    EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected(procfs::Error::UNEXPECTED_DATA);

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto readCharacters = strings::toNumber<unsigned long long>(map["rchar"]);
    const auto writeCharacters = strings::toNumber<unsigned long long>(map["wchar"]);
    const auto readSyscall = strings::toNumber<unsigned long long>(map["syscr"]);
    const auto writeSyscall = strings::toNumber<unsigned long long>(map["syscw"]);
    const auto readBytes = strings::toNumber<unsigned long long>(map["read_bytes"]);
    const auto writeBytes = strings::toNumber<unsigned long long>(map["write_bytes"]);
    const auto cancelledWriteBytes = strings::toNumber<unsigned long long>(map["cancelled_write_bytes"]);

    EXPECT(readCharacters);
    EXPECT(writeCharacters);
    EXPECT(readSyscall);
    EXPECT(writeSyscall);
    EXPECT(readBytes);
    EXPECT(writeBytes);
    EXPECT(cancelledWriteBytes);

    return IOStat{
        *readCharacters,
        *writeCharacters,
        *readSyscall,
        *writeSyscall,
        *readBytes,
        *writeBytes,
        *cancelledWriteBytes
    };
}

std::expected<zero::os::procfs::process::Process, std::error_code> zero::os::procfs::process::self() {
    return open(getpid());
}

std::expected<zero::os::procfs::process::Process, std::error_code> zero::os::procfs::process::open(const pid_t pid) {
    const auto path = std::filesystem::path("/proc") / std::to_string(pid);
    const auto fd = unix::expected([&] {
#ifdef O_PATH
        return ::open(path.string().c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC);
#else
        return ::open(path.string().c_str(), O_DIRECTORY | O_CLOEXEC);
#endif
    });
    EXPECT(fd);

    return Process{*fd, pid};
}

std::expected<std::list<pid_t>, std::error_code> zero::os::procfs::process::all() {
    std::error_code ec;
    auto iterator = std::filesystem::directory_iterator("/proc", ec);

    if (ec)
        return std::unexpected(ec);

    auto v = iterator
        | std::views::filter([&](const auto &entry) { return entry.is_directory(ec); })
        | std::views::transform([](const auto &entry) {
            return strings::toNumber<pid_t>(entry.path().filename().string());
        })
        | std::views::filter([](const auto &result) { return result.has_value(); })
        | std::views::transform([](const auto &result) { return *result; });

    std::list<pid_t> ids;
    std::ranges::copy(v, std::back_inserter(ids));

    return ids;
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::procfs::process::Process::Error)
