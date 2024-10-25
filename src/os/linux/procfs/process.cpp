#include <zero/os/linux/procfs/process.h>
#include <zero/os/linux/procfs/procfs.h>
#include <zero/detail/type_traits.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/std.h>
#include <zero/os/unix/error.h>
#include <zero/defer.h>
#include <zero/expect.h>
#include <climits>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

constexpr auto STAT_BASIC_FIELDS = 37;
constexpr auto MAPPING_BASIC_FIELDS = 5;
constexpr auto MAPPING_PERMISSIONS_LENGTH = 4;

zero::os::linux::procfs::process::Process::Process(const int fd, const pid_t pid) : mFD{fd}, mPID{pid} {
}

zero::os::linux::procfs::process::Process::Process(Process &&rhs) noexcept
    : mFD{std::exchange(rhs.mFD, -1)}, mPID{std::exchange(rhs.mPID, -1)} {
}

zero::os::linux::procfs::process::Process &
zero::os::linux::procfs::process::Process::operator=(Process &&rhs) noexcept {
    mFD = std::exchange(rhs.mFD, -1);
    mPID = std::exchange(rhs.mPID, -1);
    return *this;
}

zero::os::linux::procfs::process::Process::~Process() {
    if (mFD < 0)
        return;

    close(mFD);
}

std::expected<std::string, std::error_code>
zero::os::linux::procfs::process::Process::readFile(const char *filename) const {
    const auto fd = unix::expected([&] {
        return openat(mFD, filename, O_RDONLY);
    });
    EXPECT(fd);
    DEFER(close(*fd));

    std::string content;

    while (true) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        const auto n = unix::ensure([&] {
            return read(*fd, buffer.data(), buffer.size());
        });
        EXPECT(n);

        if (*n == 0)
            break;

        content.append(buffer.data(), *n);
    }

    return content;
}

pid_t zero::os::linux::procfs::process::Process::pid() const {
    return mPID;
}

std::expected<std::filesystem::path, std::error_code> zero::os::linux::procfs::process::Process::exe() const {
    std::array<char, PATH_MAX + 1> buffer{};

    EXPECT(unix::expected([&] {
        return readlinkat(mFD, "exe", buffer.data(), PATH_MAX);
    }));

    return buffer.data();
}

std::expected<std::filesystem::path, std::error_code> zero::os::linux::procfs::process::Process::cwd() const {
    std::array<char, PATH_MAX + 1> buffer{};

    EXPECT(unix::expected([&] {
        return readlinkat(mFD, "cwd", buffer.data(), PATH_MAX);
    }));

    return buffer.data();
}

std::expected<std::string, std::error_code> zero::os::linux::procfs::process::Process::comm() const {
    auto content = readFile("comm");
    EXPECT(content);

    if (content->size() < 2)
        return std::unexpected{procfs::Error::UNEXPECTED_DATA};

    content->pop_back();
    return *std::move(content);
}

std::expected<std::vector<std::string>, std::error_code> zero::os::linux::procfs::process::Process::cmdline() const {
    auto content = readFile("cmdline");
    EXPECT(content);

    if (content->empty())
        return std::unexpected{Error::MAYBE_ZOMBIE_PROCESS};

    content->pop_back();
    auto tokens = strings::split(*content, {"\0", 1});

    if (tokens.empty())
        return std::unexpected{procfs::Error::UNEXPECTED_DATA};

    return tokens;
}

std::expected<std::map<std::string, std::string>, std::error_code>
zero::os::linux::procfs::process::Process::environ() const {
    auto content = readFile("environ");
    EXPECT(content);

    if (content->empty())
        return {};

    content->pop_back();
    std::map<std::string, std::string> environ;

    for (const auto &env: strings::split(*content, {"\0", 1})) {
        auto tokens = strings::split(env, "=", 1);

        if (tokens.size() != 2)
            return std::unexpected<std::error_code>(procfs::Error::UNEXPECTED_DATA);

        environ.emplace(std::move(tokens[0]), std::move(tokens[1]));
    }

    return environ;
}

std::expected<zero::os::linux::procfs::process::Stat, std::error_code>
zero::os::linux::procfs::process::Process::stat() const {
    const auto content = readFile("stat");
    EXPECT(content);

    const auto start = content->find('(');
    const auto end = content->rfind(')');

    if (start == std::string::npos || end == std::string::npos)
        return std::unexpected{procfs::Error::UNEXPECTED_DATA};

    Stat stat{};

    const auto pid = strings::toNumber<std::int32_t>({content->begin(), content->begin() + start - 1});
    EXPECT(pid);

    stat.pid = *pid;
    stat.comm = content->substr(start + 1, end - start - 1);

    const auto tokens = strings::split({content->begin() + end + 2, content->end()}, " ");

    if (tokens.size() < STAT_BASIC_FIELDS - 2)
        return std::unexpected{procfs::Error::UNEXPECTED_DATA};

    auto it = tokens.begin();

    stat.state = it++->at(0);

    const auto set = [&]<typename T>(T &var) -> std::expected<void, std::error_code> {
        if constexpr (detail::is_specialization_v<T, std::optional>) {
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
    EXPECT(set(stat.processGroupID));
    EXPECT(set(stat.sessionID));
    EXPECT(set(stat.ttyNumber));
    EXPECT(set(stat.terminalProcessGroupID));
    EXPECT(set(stat.flags));
    EXPECT(set(stat.minorFaults));
    EXPECT(set(stat.childMinorFaults));
    EXPECT(set(stat.majorFaults));
    EXPECT(set(stat.childMajorFaults));
    EXPECT(set(stat.userTime));
    EXPECT(set(stat.systemTime));
    EXPECT(set(stat.childUserTime));
    EXPECT(set(stat.childSystemTime));
    EXPECT(set(stat.priority));
    EXPECT(set(stat.niceValue));
    EXPECT(set(stat.numThreads));
    EXPECT(set(stat.intervalRealValue));
    EXPECT(set(stat.startTime));
    EXPECT(set(stat.virtualMemorySize));
    EXPECT(set(stat.rss));
    EXPECT(set(stat.rssLimit));
    EXPECT(set(stat.startCode));
    EXPECT(set(stat.endCode));
    EXPECT(set(stat.startStack));
    EXPECT(set(stat.kernelStackPointer));
    EXPECT(set(stat.kernelInstructionPointer));
    EXPECT(set(stat.pendingSignals));
    EXPECT(set(stat.blockedSignals));
    EXPECT(set(stat.ignoredSignals));
    EXPECT(set(stat.caughtSignals));
    EXPECT(set(stat.waitingChannel));
    EXPECT(set(stat.pagesSwapped));
    EXPECT(set(stat.childPagesSwapped));
    EXPECT(set(stat.exitSignal));
    EXPECT(set(stat.processor));
    EXPECT(set(stat.realTimePriority));
    EXPECT(set(stat.schedulingPolicy));
    EXPECT(set(stat.blockIODelayTicks));
    EXPECT(set(stat.guestTime));
    EXPECT(set(stat.childGuestTime));
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

std::expected<zero::os::linux::procfs::process::StatM, std::error_code>
zero::os::linux::procfs::process::Process::statM() const {
    const auto content = readFile("statm");
    EXPECT(content);

    const auto tokens = strings::split(*content, " ");

    if (tokens.size() != 7)
        return std::unexpected{procfs::Error::UNEXPECTED_DATA};

    const auto size = strings::toNumber<std::uint64_t>(tokens[0]);
    EXPECT(size);

    const auto resident = strings::toNumber<std::uint64_t>(tokens[1]);
    EXPECT(resident);

    const auto shared = strings::toNumber<std::uint64_t>(tokens[2]);
    EXPECT(shared);

    const auto text = strings::toNumber<std::uint64_t>(tokens[3]);
    EXPECT(text);

    const auto library = strings::toNumber<std::uint64_t>(tokens[4]);
    EXPECT(library);

    const auto data = strings::toNumber<std::uint64_t>(tokens[5]);
    EXPECT(data);

    const auto dirty = strings::toNumber<std::uint64_t>(tokens[6]);
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
std::expected<std::vector<T>, std::error_code> parseNumbers(const std::string_view str) {
    std::vector<T> result;

    for (const auto &token: zero::strings::split(str)) {
        const auto n = zero::strings::toNumber<T>(token);
        EXPECT(n);
        result.emplace_back(*n);
    }

    return result;
}

std::expected<std::vector<std::uint32_t>, std::error_code> parseAllowed(const std::string_view str) {
    std::vector<std::uint32_t> result;

    for (const auto &token: zero::strings::split(str, ",")) {
        const auto n = zero::strings::toNumber<std::uint32_t>(token, 16);
        EXPECT(n);
        result.emplace_back(*n);
    }

    return result;
}

std::expected<std::vector<std::pair<std::uint32_t, std::uint32_t>>, std::error_code>
parseAllowedList(const std::string_view str) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> result;

    for (const auto &token: zero::strings::split(str, ",")) {
        if (!token.contains('-')) {
            const auto n = zero::strings::toNumber<std::uint32_t>(token);
            EXPECT(n);
            result.emplace_back(*n, *n);
            continue;
        }

        const auto tokens = zero::strings::split(token, "-", 1);

        if (tokens.size() != 2)
            return std::unexpected{zero::os::linux::procfs::Error::UNEXPECTED_DATA};

        const auto begin = zero::strings::toNumber<std::uint32_t>(tokens[0]);
        EXPECT(begin);

        const auto end = zero::strings::toNumber<std::uint32_t>(tokens[1]);
        EXPECT(end);

        result.emplace_back(*begin, *end);
    }

    return result;
}

std::expected<zero::os::linux::procfs::process::Status, std::error_code>
zero::os::linux::procfs::process::Process::status() const {
    const auto content = readFile("status");
    EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected{procfs::Error::UNEXPECTED_DATA};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto take = [&](const std::string &key) -> std::optional<std::string> {
        const auto it = map.find(key);

        if (it == map.end())
            return std::nullopt;

        DEFER(map.erase(it));
        return std::move(it->second);
    };

    const auto set = []<typename T, typename V>(
        T &var,
        std::optional<V> value
    ) -> std::expected<void, std::error_code> {
        if (!value) {
            if constexpr (detail::is_specialization_v<T, std::optional>)
                return {};
            else
                return std::unexpected{procfs::Error::UNEXPECTED_DATA};
        }

        if constexpr (detail::is_specialization_v<V, std::expected>) {
            EXPECT(*value);
            var = *std::move(*value);
        }
        else {
            var = *value;
        }

        return {};
    };

    const auto setNumber = [&]<typename T>(T &var, const std::string &key, const int base = 10) {
        return set(
            var,
            take(key).transform([=](const auto &value) {
                if constexpr (detail::is_specialization_v<T, std::optional>)
                    return strings::toNumber<typename T::value_type>(value, base);
                else
                    return strings::toNumber<T>(value, base);
            })
        );
    };

    Status status{};

    EXPECT(set(status.name, take("Name")));
    EXPECT(setNumber(status.umask, "Umask", 8));
    EXPECT(set(status.state, take("State")));
    EXPECT(setNumber(status.threadGroupID, "Tgid"));
    EXPECT(setNumber(status.numaGroupID, "Ngid"));
    EXPECT(setNumber(status.pid, "Pid"));
    EXPECT(setNumber(status.ppid, "PPid"));
    EXPECT(setNumber(status.tracerPID, "TracerPid"));

    const auto parseIDs = [](const auto &value) -> std::expected<std::array<std::uint32_t, 4>, std::error_code> {
        const auto numbers = parseNumbers<std::uint32_t>(value);
        EXPECT(numbers);

        if (numbers->size() != 4)
            return std::unexpected{procfs::Error::UNEXPECTED_DATA};

        return std::array{numbers->at(0), numbers->at(1), numbers->at(2), numbers->at(3)};
    };

    EXPECT(set(status.uid, take("Uid").transform(parseIDs)));
    EXPECT(set(status.gid, take("Gid").transform(parseIDs)));

    EXPECT(setNumber(status.fdSize, "FDSize"));

    EXPECT(set(status.supplementaryGroupIDs, take("Groups").transform(parseNumbers<std::int32_t>)));
    EXPECT(set(status.namespaceThreadGroupIDs, take("NStgid").transform(parseNumbers<std::int32_t>)));
    EXPECT(set(status.namespaceProcessIDs, take("NSpid").transform(parseNumbers<std::int32_t>)));
    EXPECT(set(status.namespaceProcessGroupIDs, take("NSpgid").transform(parseNumbers<std::int32_t>)));
    EXPECT(set(status.namespaceSessionIDs, take("NSsid").transform(parseNumbers<std::int32_t>)));

    EXPECT(setNumber(status.vmPeak, "VmPeak"));
    EXPECT(setNumber(status.vmSize, "VmSize"));
    EXPECT(setNumber(status.vmLocked, "VmLck"));
    EXPECT(setNumber(status.vmPinned, "VmPin"));
    EXPECT(setNumber(status.vmHWM, "VmHWM"));
    EXPECT(setNumber(status.vmRSS, "VmRSS"));
    EXPECT(setNumber(status.rssAnonymous, "RssAnon"));
    EXPECT(setNumber(status.rssFile, "RssFile"));
    EXPECT(setNumber(status.rssSharedMemory, "RssShmem"));
    EXPECT(setNumber(status.vmData, "VmData"));
    EXPECT(setNumber(status.vmStack, "VmStk"));
    EXPECT(setNumber(status.vmExe, "VmExe"));
    EXPECT(setNumber(status.vmLib, "VmLib"));
    EXPECT(setNumber(status.vmPTE, "VmPTE"));
    EXPECT(setNumber(status.vmSwap, "VmSwap"));
    EXPECT(setNumber(status.hugeTLBPages, "HugetlbPages"));
    EXPECT(setNumber(status.threads, "Threads"));

    EXPECT(set(
        status.signalQueue,
        take("SigQ").transform([](const auto &value) -> std::expected<std::array<std::uint64_t, 2>, std::error_code> {
            const auto tokens = strings::split(value, "/", 1);

            if (tokens.size() != 2)
                return std::unexpected{procfs::Error::UNEXPECTED_DATA};

            const auto number = strings::toNumber<std::uint64_t>(tokens[0]);
            EXPECT(number);

            const auto limit = strings::toNumber<std::uint64_t>(tokens[1]);
            EXPECT(limit);

            return std::array{*number, *limit};
        })
    ));

    EXPECT(setNumber(status.pendingSignals, "SigPnd", 16));
    EXPECT(setNumber(status.blockedSignals, "SigBlk", 16));
    EXPECT(setNumber(status.ignoredSignals, "SigIgn", 16));
    EXPECT(setNumber(status.caughtSignals, "SigCgt", 16));
    EXPECT(setNumber(status.inheritableCapabilities, "CapInh", 16));
    EXPECT(setNumber(status.permittedCapabilities, "CapPrm", 16));
    EXPECT(setNumber(status.effectiveCapabilities, "CapEff", 16));
    EXPECT(setNumber(status.boundingCapabilities, "CapBnd", 16));
    EXPECT(setNumber(status.ambientCapabilities, "CapAmb", 16));
    EXPECT(setNumber(status.noNewPrivileges, "NoNewPrivs"));
    EXPECT(setNumber(status.seccompMode, "Seccomp"));

    EXPECT(set(status.speculationStoreBypass, take("Speculation_Store_Bypass")));

    EXPECT(set(status.allowedCPUs, take("Cpus_allowed").transform(parseAllowed)));
    EXPECT(set(status.allowedCPUList, take("Cpus_allowed_list").transform(parseAllowedList)));
    EXPECT(set(status.allowedMemoryNodes, take("Mems_allowed").transform(parseAllowed)));
    EXPECT(set(status.allowedMemoryNodeList, take("Mems_allowed_list").transform(parseAllowedList)));

    EXPECT(setNumber(status.voluntaryContextSwitches, "voluntary_ctxt_switches"));
    EXPECT(setNumber(status.nonVoluntaryContextSwitches, "nonvoluntary_ctxt_switches"));

    EXPECT(set(status.coreDumping, take("CoreDumping").transform([](const auto &value) { return value == "1"; })));
    EXPECT(set(status.thpEnabled, take("THP_enabled").transform([](const auto &value) { return value == "1"; })));

    return status;
}

std::expected<std::list<pid_t>, std::error_code> zero::os::linux::procfs::process::Process::tasks() const {
    using namespace std::string_view_literals;

    const auto fd = unix::expected([&] {
        return openat(mFD, "task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    });
    EXPECT(fd);

    const auto dir = fdopendir(*fd);

    if (!dir) {
        close(*fd);
        return std::unexpected{std::error_code{errno, std::system_category()}};
    }

    DEFER(closedir(dir));
    std::list<pid_t> tasks;

    while (true) {
        const auto *e = readdir(dir);

        if (!e)
            break;

        if (e->d_name == "."sv || e->d_name == ".."sv)
            continue;

        const auto id = strings::toNumber<pid_t>(e->d_name);
        EXPECT(id);

        tasks.push_back(*id);
    }

    return tasks;
}

std::expected<std::list<zero::os::linux::procfs::process::MemoryMapping>, std::error_code>
zero::os::linux::procfs::process::Process::maps() const {
    const auto content = readFile("maps");
    EXPECT(content);

    if (content->empty())
        return std::unexpected{Error::MAYBE_ZOMBIE_PROCESS};

    std::list<MemoryMapping> mappings;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        const auto fields = strings::split(line);

        if (fields.size() < MAPPING_BASIC_FIELDS)
            return std::unexpected{procfs::Error::UNEXPECTED_DATA};

        const auto tokens = strings::split(fields[0], "-", 1);

        if (tokens.size() != 2)
            return std::unexpected{procfs::Error::UNEXPECTED_DATA};

        const auto start = strings::toNumber<std::uint64_t>(tokens[0], 16);
        EXPECT(start);

        const auto end = strings::toNumber<std::uint64_t>(tokens[1], 16);
        EXPECT(end);

        const auto offset = strings::toNumber<std::uint64_t>(fields[2], 16);
        EXPECT(offset);

        const auto inode = strings::toNumber<std::uint64_t>(fields[4]);
        EXPECT(inode);

        MemoryMapping memoryMapping{};

        memoryMapping.start = *start;
        memoryMapping.end = *end;
        memoryMapping.offset = *offset;
        memoryMapping.inode = *inode;
        memoryMapping.device = fields[3];

        if (fields.size() > MAPPING_BASIC_FIELDS)
            memoryMapping.pathname = fields[5];

        const auto &permissions = fields[1];

        if (permissions.length() < MAPPING_PERMISSIONS_LENGTH)
            return std::unexpected<std::error_code>(procfs::Error::UNEXPECTED_DATA);

        if (permissions.at(0) == 'r')
            memoryMapping.permissions.set(READ);

        if (permissions.at(1) == 'w')
            memoryMapping.permissions.set(WRITE);

        if (permissions.at(2) == 'x')
            memoryMapping.permissions.set(EXECUTE);

        if (permissions.at(3) == 's')
            memoryMapping.permissions.set(SHARED);

        if (permissions.at(3) == 'p')
            memoryMapping.permissions.set(PRIVATE);

        mappings.push_back(std::move(memoryMapping));
    }

    return mappings;
}

std::expected<zero::os::linux::procfs::process::IOStat, std::error_code>
zero::os::linux::procfs::process::Process::io() const {
    const auto content = readFile("io");
    EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected{procfs::Error::UNEXPECTED_DATA};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto set = [&]<typename T>(T &var, const std::string &key) -> std::expected<void, std::error_code> {
        const auto it = map.find(key);

        if (it == map.end())
            return std::unexpected{procfs::Error::UNEXPECTED_DATA};

        const auto value = strings::toNumber<T>(it->second);
        EXPECT(value);
        var = *value;
        return {};
    };

    IOStat stat{};

    EXPECT(set(stat.readCharacters, "rchar"));
    EXPECT(set(stat.writeCharacters, "wchar"));
    EXPECT(set(stat.readSyscalls, "syscr"));
    EXPECT(set(stat.writeSyscalls, "syscw"));
    EXPECT(set(stat.readBytes, "read_bytes"));
    EXPECT(set(stat.writeBytes, "write_bytes"));
    EXPECT(set(stat.cancelledWriteBytes, "cancelled_write_bytes"));

    return stat;
}

std::expected<zero::os::linux::procfs::process::Process, std::error_code> zero::os::linux::procfs::process::self() {
    return open(getpid());
}

std::expected<zero::os::linux::procfs::process::Process, std::error_code>
zero::os::linux::procfs::process::open(const pid_t pid) {
    const auto path = std::filesystem::path{"/proc"} / std::to_string(pid);
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

std::expected<std::list<pid_t>, std::error_code> zero::os::linux::procfs::process::all() {
    const auto iterator = filesystem::readDirectory("/proc");
    EXPECT(iterator);

    std::list<pid_t> ids;

    for (const auto &entry: *iterator) {
        EXPECT(entry);

        if (!entry->isDirectory().value_or(false))
            continue;

        const auto id = strings::toNumber<pid_t>(entry->path().filename().string());

        if (!id)
            continue;

        ids.push_back(*id);
    }

    return ids;
}

DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::linux::procfs::process::Process::Error)
