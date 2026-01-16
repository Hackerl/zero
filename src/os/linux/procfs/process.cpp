#include <zero/os/linux/procfs/process.h>
#include <zero/os/linux/procfs/procfs.h>
#include <zero/traits/type_traits.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/fs.h>
#include <zero/os/unix/error.h>
#include <zero/defer.h>
#include <zero/expect.h>
#include <climits>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

constexpr auto StatBasicFields = 37;
constexpr auto MappingBasicFields = 5;
constexpr auto MappingPermissionsLength = 4;

zero::os::linux::procfs::process::Process::Process(Resource resource, const pid_t pid)
    : mResource{std::move(resource)}, mPID{pid} {
}

std::expected<std::string, std::error_code>
zero::os::linux::procfs::process::Process::readFile(const char *filename) const {
    const auto resource = unix::expected([&] {
        return openat(*mResource, filename, O_RDONLY);
    }).transform([](const auto &fd) {
        return Resource{fd};
    });
    Z_EXPECT(resource);

    std::string content;

    while (true) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        const auto n = unix::ensure([&] {
            return read(resource->get(), buffer.data(), buffer.size());
        });
        Z_EXPECT(n);

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

    Z_EXPECT(unix::expected([&] {
        return readlinkat(*mResource, "exe", buffer.data(), PATH_MAX);
    }));

    return buffer.data();
}

std::expected<std::filesystem::path, std::error_code> zero::os::linux::procfs::process::Process::cwd() const {
    std::array<char, PATH_MAX + 1> buffer{};

    Z_EXPECT(unix::expected([&] {
        return readlinkat(*mResource, "cwd", buffer.data(), PATH_MAX);
    }));

    return buffer.data();
}

std::expected<std::string, std::error_code> zero::os::linux::procfs::process::Process::comm() const {
    auto content = readFile("comm");
    Z_EXPECT(content);

    if (content->size() < 2)
        return std::unexpected{procfs::Error::UnexpectedData};

    content->pop_back();
    return *std::move(content);
}

std::expected<std::vector<std::string>, std::error_code> zero::os::linux::procfs::process::Process::cmdline() const {
    auto content = readFile("cmdline");
    Z_EXPECT(content);

    if (content->empty())
        return std::unexpected{Error::MAYBE_ZOMBIE_PROCESS};

    content->pop_back();
    auto tokens = strings::split(*content, {"\0", 1});

    if (tokens.empty())
        return std::unexpected{procfs::Error::UnexpectedData};

    return tokens;
}

std::expected<std::map<std::string, std::string>, std::error_code>
zero::os::linux::procfs::process::Process::environ() const {
    auto content = readFile("environ");
    Z_EXPECT(content);

    if (content->empty())
        return {};

    content->pop_back();
    std::map<std::string, std::string> environ;

    for (const auto &env: strings::split(*content, {"\0", 1})) {
        auto tokens = strings::split(env, "=", 1);

        if (tokens.size() != 2)
            return std::unexpected<std::error_code>(procfs::Error::UnexpectedData);

        environ.emplace(std::move(tokens[0]), std::move(tokens[1]));
    }

    return environ;
}

std::expected<zero::os::linux::procfs::process::Stat, std::error_code>
zero::os::linux::procfs::process::Process::stat() const {
    const auto content = readFile("stat");
    Z_EXPECT(content);

    const auto start = content->find('(');
    const auto end = content->rfind(')');

    if (start == std::string::npos || end == std::string::npos)
        return std::unexpected{procfs::Error::UnexpectedData};

    Stat stat;

    const auto pid = strings::toNumber<std::int32_t>({
        content->begin(), content->begin() + static_cast<std::ptrdiff_t>(start) - 1
    });
    Z_EXPECT(pid);

    stat.pid = *pid;
    stat.comm = content->substr(start + 1, end - start - 1);

    const auto tokens = strings::split({content->begin() + static_cast<std::ptrdiff_t>(end) + 2, content->end()}, " ");

    if (tokens.size() < StatBasicFields - 2)
        return std::unexpected{procfs::Error::UnexpectedData};

    auto it = tokens.begin();

    stat.state = it++->at(0);

    const auto set = [&]<typename T>(T &var) -> std::expected<void, std::error_code> {
        if constexpr (traits::is_specialization_v<T, std::optional>) {
            if (it == tokens.end())
                return {};

            const auto value = strings::toNumber<typename T::value_type>(*it++);
            Z_EXPECT(value);
            var = *value;
        }
        else {
            const auto value = strings::toNumber<T>(*it++);
            Z_EXPECT(value);
            var = *value;
        }

        return {};
    };

    Z_EXPECT(set(stat.ppid));
    Z_EXPECT(set(stat.processGroupID));
    Z_EXPECT(set(stat.sessionID));
    Z_EXPECT(set(stat.ttyNumber));
    Z_EXPECT(set(stat.terminalProcessGroupID));
    Z_EXPECT(set(stat.flags));
    Z_EXPECT(set(stat.minorFaults));
    Z_EXPECT(set(stat.childMinorFaults));
    Z_EXPECT(set(stat.majorFaults));
    Z_EXPECT(set(stat.childMajorFaults));
    Z_EXPECT(set(stat.userTime));
    Z_EXPECT(set(stat.systemTime));
    Z_EXPECT(set(stat.childUserTime));
    Z_EXPECT(set(stat.childSystemTime));
    Z_EXPECT(set(stat.priority));
    Z_EXPECT(set(stat.niceValue));
    Z_EXPECT(set(stat.numThreads));
    Z_EXPECT(set(stat.intervalRealValue));
    Z_EXPECT(set(stat.startTime));
    Z_EXPECT(set(stat.virtualMemorySize));
    Z_EXPECT(set(stat.rss));
    Z_EXPECT(set(stat.rssLimit));
    Z_EXPECT(set(stat.startCode));
    Z_EXPECT(set(stat.endCode));
    Z_EXPECT(set(stat.startStack));
    Z_EXPECT(set(stat.kernelStackPointer));
    Z_EXPECT(set(stat.kernelInstructionPointer));
    Z_EXPECT(set(stat.pendingSignals));
    Z_EXPECT(set(stat.blockedSignals));
    Z_EXPECT(set(stat.ignoredSignals));
    Z_EXPECT(set(stat.caughtSignals));
    Z_EXPECT(set(stat.waitingChannel));
    Z_EXPECT(set(stat.pagesSwapped));
    Z_EXPECT(set(stat.childPagesSwapped));
    Z_EXPECT(set(stat.exitSignal));
    Z_EXPECT(set(stat.processor));
    Z_EXPECT(set(stat.realTimePriority));
    Z_EXPECT(set(stat.schedulingPolicy));
    Z_EXPECT(set(stat.blockIODelayTicks));
    Z_EXPECT(set(stat.guestTime));
    Z_EXPECT(set(stat.childGuestTime));
    Z_EXPECT(set(stat.startData));
    Z_EXPECT(set(stat.endData));
    Z_EXPECT(set(stat.startBrk));
    Z_EXPECT(set(stat.argStart));
    Z_EXPECT(set(stat.argEnd));
    Z_EXPECT(set(stat.envStart));
    Z_EXPECT(set(stat.envEnd));
    Z_EXPECT(set(stat.exitCode));

    return stat;
}

std::expected<zero::os::linux::procfs::process::StatM, std::error_code>
zero::os::linux::procfs::process::Process::statM() const {
    const auto content = readFile("statm");
    Z_EXPECT(content);

    const auto tokens = strings::split(*content, " ");

    if (tokens.size() != 7)
        return std::unexpected{procfs::Error::UnexpectedData};

    const auto size = strings::toNumber<std::uint64_t>(tokens[0]);
    Z_EXPECT(size);

    const auto resident = strings::toNumber<std::uint64_t>(tokens[1]);
    Z_EXPECT(resident);

    const auto shared = strings::toNumber<std::uint64_t>(tokens[2]);
    Z_EXPECT(shared);

    const auto text = strings::toNumber<std::uint64_t>(tokens[3]);
    Z_EXPECT(text);

    const auto library = strings::toNumber<std::uint64_t>(tokens[4]);
    Z_EXPECT(library);

    const auto data = strings::toNumber<std::uint64_t>(tokens[5]);
    Z_EXPECT(data);

    const auto dirty = strings::toNumber<std::uint64_t>(tokens[6]);
    Z_EXPECT(dirty);

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
        Z_EXPECT(n);
        result.emplace_back(*n);
    }

    return result;
}

namespace {
    std::expected<std::vector<std::uint32_t>, std::error_code> parseAllowed(const std::string_view str) {
        std::vector<std::uint32_t> result;

        for (const auto &token: zero::strings::split(str, ",")) {
            const auto n = zero::strings::toNumber<std::uint32_t>(token, 16);
            Z_EXPECT(n);
            result.emplace_back(*n);
        }

        return result;
    }
}

std::expected<std::vector<std::pair<std::uint32_t, std::uint32_t>>, std::error_code>
parseAllowedList(const std::string_view str) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> result;

    for (const auto &token: zero::strings::split(str, ",")) {
        if (!token.contains('-')) {
            const auto n = zero::strings::toNumber<std::uint32_t>(token);
            Z_EXPECT(n);
            result.emplace_back(*n, *n);
            continue;
        }

        const auto tokens = zero::strings::split(token, "-", 1);

        if (tokens.size() != 2)
            return std::unexpected{zero::os::linux::procfs::Error::UnexpectedData};

        const auto begin = zero::strings::toNumber<std::uint32_t>(tokens[0]);
        Z_EXPECT(begin);

        const auto end = zero::strings::toNumber<std::uint32_t>(tokens[1]);
        Z_EXPECT(end);

        result.emplace_back(*begin, *end);
    }

    return result;
}

std::expected<zero::os::linux::procfs::process::Status, std::error_code>
zero::os::linux::procfs::process::Process::status() const {
    const auto content = readFile("status");
    Z_EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected{procfs::Error::UnexpectedData};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto take = [&](const std::string &key) -> std::optional<std::string> {
        const auto it = map.find(key);

        if (it == map.end())
            return std::nullopt;

        Z_DEFER(map.erase(it));
        return std::move(it->second);
    };

    const auto set = []<typename T, typename V>(
        T &var,
        std::optional<V> value
    ) -> std::expected<void, std::error_code> {
        if (!value) {
            if constexpr (traits::is_specialization_v<T, std::optional>)
                return {};
            else
                return std::unexpected{procfs::Error::UnexpectedData};
        }

        if constexpr (traits::is_specialization_v<V, std::expected>) {
            Z_EXPECT(*value);
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
                if constexpr (traits::is_specialization_v<T, std::optional>)
                    return strings::toNumber<typename T::value_type>(value, base);
                else
                    return strings::toNumber<T>(value, base);
            })
        );
    };

    Status status;

    Z_EXPECT(set(status.name, take("Name")));
    Z_EXPECT(setNumber(status.umask, "Umask", 8));
    Z_EXPECT(set(status.state, take("State")));
    Z_EXPECT(setNumber(status.threadGroupID, "Tgid"));
    Z_EXPECT(setNumber(status.numaGroupID, "Ngid"));
    Z_EXPECT(setNumber(status.pid, "Pid"));
    Z_EXPECT(setNumber(status.ppid, "PPid"));
    Z_EXPECT(setNumber(status.tracerPID, "TracerPid"));

    const auto parseIDs = [](const auto &value) -> std::expected<std::array<std::uint32_t, 4>, std::error_code> {
        const auto numbers = parseNumbers<std::uint32_t>(value);
        Z_EXPECT(numbers);

        if (numbers->size() != 4)
            return std::unexpected{procfs::Error::UnexpectedData};

        return std::array{numbers->at(0), numbers->at(1), numbers->at(2), numbers->at(3)};
    };

    Z_EXPECT(set(status.uid, take("Uid").transform(parseIDs)));
    Z_EXPECT(set(status.gid, take("Gid").transform(parseIDs)));

    Z_EXPECT(setNumber(status.fdSize, "FDSize"));

    Z_EXPECT(set(status.supplementaryGroupIDs, take("Groups").transform(parseNumbers<std::int32_t>)));
    Z_EXPECT(set(status.namespaceThreadGroupIDs, take("NStgid").transform(parseNumbers<std::int32_t>)));
    Z_EXPECT(set(status.namespaceProcessIDs, take("NSpid").transform(parseNumbers<std::int32_t>)));
    Z_EXPECT(set(status.namespaceProcessGroupIDs, take("NSpgid").transform(parseNumbers<std::int32_t>)));
    Z_EXPECT(set(status.namespaceSessionIDs, take("NSsid").transform(parseNumbers<std::int32_t>)));

    Z_EXPECT(setNumber(status.vmPeak, "VmPeak"));
    Z_EXPECT(setNumber(status.vmSize, "VmSize"));
    Z_EXPECT(setNumber(status.vmLocked, "VmLck"));
    Z_EXPECT(setNumber(status.vmPinned, "VmPin"));
    Z_EXPECT(setNumber(status.vmHWM, "VmHWM"));
    Z_EXPECT(setNumber(status.vmRSS, "VmRSS"));
    Z_EXPECT(setNumber(status.rssAnonymous, "RssAnon"));
    Z_EXPECT(setNumber(status.rssFile, "RssFile"));
    Z_EXPECT(setNumber(status.rssSharedMemory, "RssShmem"));
    Z_EXPECT(setNumber(status.vmData, "VmData"));
    Z_EXPECT(setNumber(status.vmStack, "VmStk"));
    Z_EXPECT(setNumber(status.vmExe, "VmExe"));
    Z_EXPECT(setNumber(status.vmLib, "VmLib"));
    Z_EXPECT(setNumber(status.vmPTE, "VmPTE"));
    Z_EXPECT(setNumber(status.vmSwap, "VmSwap"));
    Z_EXPECT(setNumber(status.hugeTLBPages, "HugetlbPages"));
    Z_EXPECT(setNumber(status.threads, "Threads"));

    Z_EXPECT(set(
        status.signalQueue,
        take("SigQ").transform([](const auto &value) -> std::expected<std::array<std::uint64_t, 2>, std::error_code> {
            const auto tokens = strings::split(value, "/", 1);

            if (tokens.size() != 2)
                return std::unexpected{procfs::Error::UnexpectedData};

            const auto number = strings::toNumber<std::uint64_t>(tokens[0]);
            Z_EXPECT(number);

            const auto limit = strings::toNumber<std::uint64_t>(tokens[1]);
            Z_EXPECT(limit);

            return std::array{*number, *limit};
        })
    ));

    Z_EXPECT(setNumber(status.pendingSignals, "SigPnd", 16));
    Z_EXPECT(setNumber(status.blockedSignals, "SigBlk", 16));
    Z_EXPECT(setNumber(status.ignoredSignals, "SigIgn", 16));
    Z_EXPECT(setNumber(status.caughtSignals, "SigCgt", 16));
    Z_EXPECT(setNumber(status.inheritableCapabilities, "CapInh", 16));
    Z_EXPECT(setNumber(status.permittedCapabilities, "CapPrm", 16));
    Z_EXPECT(setNumber(status.effectiveCapabilities, "CapEff", 16));
    Z_EXPECT(setNumber(status.boundingCapabilities, "CapBnd", 16));
    Z_EXPECT(setNumber(status.ambientCapabilities, "CapAmb", 16));
    Z_EXPECT(setNumber(status.noNewPrivileges, "NoNewPrivs"));
    Z_EXPECT(setNumber(status.seccompMode, "Seccomp"));

    Z_EXPECT(set(status.speculationStoreBypass, take("Speculation_Store_Bypass")));

    Z_EXPECT(set(status.allowedCPUs, take("Cpus_allowed").transform(parseAllowed)));
    Z_EXPECT(set(status.allowedCPUList, take("Cpus_allowed_list").transform(parseAllowedList)));
    Z_EXPECT(set(status.allowedMemoryNodes, take("Mems_allowed").transform(parseAllowed)));
    Z_EXPECT(set(status.allowedMemoryNodeList, take("Mems_allowed_list").transform(parseAllowedList)));

    Z_EXPECT(setNumber(status.voluntaryContextSwitches, "voluntary_ctxt_switches"));
    Z_EXPECT(setNumber(status.nonVoluntaryContextSwitches, "nonvoluntary_ctxt_switches"));

    Z_EXPECT(set(status.coreDumping, take("CoreDumping").transform([](const auto &value) { return value == "1"; })));
    Z_EXPECT(set(status.thpEnabled, take("THP_enabled").transform([](const auto &value) { return value == "1"; })));

    return status;
}

std::expected<std::list<pid_t>, std::error_code> zero::os::linux::procfs::process::Process::tasks() const {
    using namespace std::string_view_literals;

    const auto fd = unix::expected([&] {
        return openat(*mResource, "task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    });
    Z_EXPECT(fd);

    const auto dir = fdopendir(*fd);

    if (!dir) {
        error::guard(unix::expected([&] {
            return close(*fd);
        }));
        return std::unexpected{std::error_code{errno, std::system_category()}};
    }

    Z_DEFER(error::guard(unix::expected([&] {
        return closedir(dir);
    })));

    std::list<pid_t> tasks;
    errno = 0;

    while (true) {
        const auto *e = readdir(dir);

        if (!e) {
            if (errno != 0)
                return std::unexpected{std::error_code{errno, std::system_category()}};

            break;
        }

        if (e->d_name == "."sv || e->d_name == ".."sv)
            continue;

        const auto id = strings::toNumber<pid_t>(e->d_name);
        Z_EXPECT(id);

        tasks.push_back(*id);
    }

    return tasks;
}

std::expected<std::list<zero::os::linux::procfs::process::MemoryMapping>, std::error_code>
zero::os::linux::procfs::process::Process::maps() const {
    const auto content = readFile("maps");
    Z_EXPECT(content);

    if (content->empty())
        return std::unexpected{Error::MAYBE_ZOMBIE_PROCESS};

    std::list<MemoryMapping> mappings;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        const auto fields = strings::split(line);

        if (fields.size() < MappingBasicFields)
            return std::unexpected{procfs::Error::UnexpectedData};

        const auto tokens = strings::split(fields[0], "-", 1);

        if (tokens.size() != 2)
            return std::unexpected{procfs::Error::UnexpectedData};

        const auto start = strings::toNumber<std::uint64_t>(tokens[0], 16);
        Z_EXPECT(start);

        const auto end = strings::toNumber<std::uint64_t>(tokens[1], 16);
        Z_EXPECT(end);

        const auto offset = strings::toNumber<std::uint64_t>(fields[2], 16);
        Z_EXPECT(offset);

        const auto inode = strings::toNumber<std::uint64_t>(fields[4]);
        Z_EXPECT(inode);

        MemoryMapping memoryMapping;

        memoryMapping.start = *start;
        memoryMapping.end = *end;
        memoryMapping.offset = *offset;
        memoryMapping.inode = *inode;
        memoryMapping.device = fields[3];

        if (fields.size() > MappingBasicFields)
            memoryMapping.pathname = fields[5];

        const auto &permissions = fields[1];

        if (permissions.length() < MappingPermissionsLength)
            return std::unexpected<std::error_code>(procfs::Error::UnexpectedData);

        if (permissions.at(0) == 'r')
            memoryMapping.permissions.set(Read);

        if (permissions.at(1) == 'w')
            memoryMapping.permissions.set(Write);

        if (permissions.at(2) == 'x')
            memoryMapping.permissions.set(Execute);

        if (permissions.at(3) == 's')
            memoryMapping.permissions.set(Shared);

        if (permissions.at(3) == 'p')
            memoryMapping.permissions.set(Private);

        mappings.push_back(std::move(memoryMapping));
    }

    return mappings;
}

std::expected<zero::os::linux::procfs::process::IOStat, std::error_code>
zero::os::linux::procfs::process::Process::io() const {
    const auto content = readFile("io");
    Z_EXPECT(content);

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(strings::trim(*content), "\n")) {
        auto tokens = strings::split(line, ":", 1);

        if (tokens.size() != 2)
            return std::unexpected{procfs::Error::UnexpectedData};

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto set = [&]<typename T>(T &var, const std::string &key) -> std::expected<void, std::error_code> {
        const auto it = map.find(key);

        if (it == map.end())
            return std::unexpected{procfs::Error::UnexpectedData};

        const auto value = strings::toNumber<T>(it->second);
        Z_EXPECT(value);
        var = *value;
        return {};
    };

    IOStat stat;

    Z_EXPECT(set(stat.readCharacters, "rchar"));
    Z_EXPECT(set(stat.writeCharacters, "wchar"));
    Z_EXPECT(set(stat.readSyscalls, "syscr"));
    Z_EXPECT(set(stat.writeSyscalls, "syscw"));
    Z_EXPECT(set(stat.readBytes, "read_bytes"));
    Z_EXPECT(set(stat.writeBytes, "write_bytes"));
    Z_EXPECT(set(stat.cancelledWriteBytes, "cancelled_write_bytes"));

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
        return ::open(path.c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC);
#else
        return ::open(path.c_str(), O_DIRECTORY | O_CLOEXEC);
#endif
    });
    Z_EXPECT(fd);

    return Process{Resource{*fd}, pid};
}

std::expected<std::list<pid_t>, std::error_code> zero::os::linux::procfs::process::all() {
    const auto iterator = filesystem::readDirectory("/proc");
    Z_EXPECT(iterator);

    std::list<pid_t> ids;

    for (const auto &entry: *iterator) {
        Z_EXPECT(entry);

        const auto result = entry->isDirectory();
        Z_EXPECT(result);

        if (!*result)
            continue;

        const auto id = strings::toNumber<pid_t>(entry->path().filename().native());

        if (!id)
            continue;

        ids.push_back(*id);
    }

    return ids;
}

Z_DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::linux::procfs::process::Process::Error)
