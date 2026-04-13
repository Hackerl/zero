#include <zero/os/linux/procfs/process.h>
#include <zero/os/linux/procfs/procfs.h>
#include <zero/os/unix/error.h>
#include <zero/meta/type_traits.h>
#include <zero/strings.h>
#include <zero/filesystem.h>
#include <zero/defer.h>
#include <zero/expect.h>
#include <fmt/format.h>
#include <ranges>
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
        throw error::StacktraceError<std::runtime_error>{
            fmt::format("Unexpected content in /proc/{}/comm: too short ({} bytes)", mPID, content->size())
        };

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
        throw error::StacktraceError<std::runtime_error>{
            fmt::format("Failed to parse /proc/{}/cmdline: empty result after split", mPID)
        };

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
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed entry in /proc/{}/environ: missing '=' separator", mPID)
            };

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
        throw error::StacktraceError<std::runtime_error>{
            fmt::format("Malformed /proc/{}/stat: missing parentheses around process name", mPID)
        };

    Stat stat;

    stat.pid = error::guard(strings::toNumber<std::int32_t>({
        content->begin(), content->begin() + static_cast<std::ptrdiff_t>(start) - 1
    }));

    stat.comm = content->substr(start + 1, end - start - 1);

    const auto tokens = strings::split({content->begin() + static_cast<std::ptrdiff_t>(end) + 2, content->end()}, " ");

    if (tokens.size() < StatBasicFields - 2)
        throw error::StacktraceError<std::runtime_error>{
            fmt::format(
                "Malformed /proc/{}/stat: expected >= {} fields, got {}",
                mPID,
                StatBasicFields - 2,
                tokens.size()
            )
        };

    auto it = tokens.begin();

    stat.state = it++->at(0);

    const auto set = [&]<typename T>(T &var) {
        if constexpr (meta::IsSpecialization<T, std::optional>) {
            if (it == tokens.end())
                return;

            var = error::guard(strings::toNumber<typename T::value_type>(*it++));
        }
        else {
            var = error::guard(strings::toNumber<T>(*it++));
        }
    };

    set(stat.ppid);
    set(stat.processGroupID);
    set(stat.sessionID);
    set(stat.ttyNumber);
    set(stat.terminalProcessGroupID);
    set(stat.flags);
    set(stat.minorFaults);
    set(stat.childMinorFaults);
    set(stat.majorFaults);
    set(stat.childMajorFaults);
    set(stat.userTime);
    set(stat.systemTime);
    set(stat.childUserTime);
    set(stat.childSystemTime);
    set(stat.priority);
    set(stat.niceValue);
    set(stat.numThreads);
    set(stat.intervalRealValue);
    set(stat.startTime);
    set(stat.virtualMemorySize);
    set(stat.rss);
    set(stat.rssLimit);
    set(stat.startCode);
    set(stat.endCode);
    set(stat.startStack);
    set(stat.kernelStackPointer);
    set(stat.kernelInstructionPointer);
    set(stat.pendingSignals);
    set(stat.blockedSignals);
    set(stat.ignoredSignals);
    set(stat.caughtSignals);
    set(stat.waitingChannel);
    set(stat.pagesSwapped);
    set(stat.childPagesSwapped);
    set(stat.exitSignal);
    set(stat.processor);
    set(stat.realTimePriority);
    set(stat.schedulingPolicy);
    set(stat.blockIODelayTicks);
    set(stat.guestTime);
    set(stat.childGuestTime);
    set(stat.startData);
    set(stat.endData);
    set(stat.startBrk);
    set(stat.argStart);
    set(stat.argEnd);
    set(stat.envStart);
    set(stat.envEnd);
    set(stat.exitCode);

    return stat;
}

std::expected<zero::os::linux::procfs::process::StatM, std::error_code>
zero::os::linux::procfs::process::Process::statM() const {
    const auto content = readFile("statm");
    Z_EXPECT(content);

    const auto tokens = strings::split(*content, " ");

    if (tokens.size() != 7)
        throw error::StacktraceError<std::runtime_error>{
            fmt::format("Malformed /proc/{}/statm: expected 7 fields, got {}", mPID, tokens.size())
        };

    return StatM{
        error::guard(strings::toNumber<std::uint64_t>(tokens[0])),
        error::guard(strings::toNumber<std::uint64_t>(tokens[1])),
        error::guard(strings::toNumber<std::uint64_t>(tokens[2])),
        error::guard(strings::toNumber<std::uint64_t>(tokens[3])),
        error::guard(strings::toNumber<std::uint64_t>(tokens[4])),
        error::guard(strings::toNumber<std::uint64_t>(tokens[5])),
        error::guard(strings::toNumber<std::uint64_t>(tokens[6]))
    };
}

template<typename T>
std::vector<T> parseNumbers(const std::string_view str) {
    return zero::strings::split(str)
        | std::views::transform([](const auto &token) {
            return zero::error::guard(zero::strings::toNumber<T>(token));
        })
        | std::ranges::to<std::vector>();
}

namespace {
    std::vector<std::uint32_t> parseAllowed(const std::string_view str) {
        return zero::strings::split(str, ",")
            | std::views::transform([](const auto &token) {
                return zero::error::guard(zero::strings::toNumber<std::uint32_t>(token, 16));
            })
            | std::ranges::to<std::vector>();
    }
}

std::vector<std::pair<std::uint32_t, std::uint32_t>>
parseAllowedList(const std::string_view str) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> result;

    for (const auto &token: zero::strings::split(str, ",")) {
        if (!token.contains('-')) {
            const auto n = zero::error::guard(zero::strings::toNumber<std::uint32_t>(token));
            result.emplace_back(n, n);
            continue;
        }

        const auto tokens = zero::strings::split(token, "-", 1);

        if (tokens.size() != 2)
            throw zero::error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed range in allowed list: '{}'", token)
            };

        result.emplace_back(
            zero::error::guard(zero::strings::toNumber<std::uint32_t>(tokens[0])),
            zero::error::guard(zero::strings::toNumber<std::uint32_t>(tokens[1]))
        );
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
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed line in /proc/{}/status: missing ':' separator", mPID)
            };

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto take = [&](const std::string &key) -> std::optional<std::string> {
        const auto it = map.find(key);

        if (it == map.end())
            return std::nullopt;

        Z_DEFER(map.erase(it));
        return std::move(it->second);
    };

    const auto set = []<typename T, typename V>(T &var, std::optional<V> value) {
        if (!value)
            return;

        var = *value;
    };

    const auto setNumber = [&]<typename T>(T &var, const std::string &key, const int base = 10) {
        return set(
            var,
            take(key).transform([=](const auto &value) {
                if constexpr (meta::IsSpecialization<T, std::optional>)
                    return error::guard(strings::toNumber<typename T::value_type>(value, base));
                else
                    return error::guard(strings::toNumber<T>(value, base));
            })
        );
    };

    Status status;

    set(status.name, take("Name"));
    setNumber(status.umask, "Umask", 8);
    set(status.state, take("State"));
    setNumber(status.threadGroupID, "Tgid");
    setNumber(status.numaGroupID, "Ngid");
    setNumber(status.pid, "Pid");
    setNumber(status.ppid, "PPid");
    setNumber(status.tracerPID, "TracerPid");

    const auto parseIDs = [](const auto &value) {
        const auto numbers = parseNumbers<std::uint32_t>(value);

        if (numbers.size() != 4)
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed UID/GID entry: expected 4 values, got {}", numbers.size())
            };

        return std::array{numbers.at(0), numbers.at(1), numbers.at(2), numbers.at(3)};
    };

    set(status.uid, take("Uid").transform(parseIDs));
    set(status.gid, take("Gid").transform(parseIDs));

    setNumber(status.fdSize, "FDSize");

    set(status.supplementaryGroupIDs, take("Groups").transform(parseNumbers<std::int32_t>));
    set(status.namespaceThreadGroupIDs, take("NStgid").transform(parseNumbers<std::int32_t>));
    set(status.namespaceProcessIDs, take("NSpid").transform(parseNumbers<std::int32_t>));
    set(status.namespaceProcessGroupIDs, take("NSpgid").transform(parseNumbers<std::int32_t>));
    set(status.namespaceSessionIDs, take("NSsid").transform(parseNumbers<std::int32_t>));

    setNumber(status.vmPeak, "VmPeak");
    setNumber(status.vmSize, "VmSize");
    setNumber(status.vmLocked, "VmLck");
    setNumber(status.vmPinned, "VmPin");
    setNumber(status.vmHWM, "VmHWM");
    setNumber(status.vmRSS, "VmRSS");
    setNumber(status.rssAnonymous, "RssAnon");
    setNumber(status.rssFile, "RssFile");
    setNumber(status.rssSharedMemory, "RssShmem");
    setNumber(status.vmData, "VmData");
    setNumber(status.vmStack, "VmStk");
    setNumber(status.vmExe, "VmExe");
    setNumber(status.vmLib, "VmLib");
    setNumber(status.vmPTE, "VmPTE");
    setNumber(status.vmSwap, "VmSwap");
    setNumber(status.hugeTLBPages, "HugetlbPages");
    setNumber(status.threads, "Threads");

    set(
        status.signalQueue,
        take("SigQ").transform([](const auto &value) {
            const auto tokens = strings::split(value, "/", 1);

            if (tokens.size() != 2)
                throw error::StacktraceError<std::runtime_error>{"Malformed SigQ field: expected 'current/max' format"};

            return std::array{
                error::guard(strings::toNumber<std::uint64_t>(tokens[0])),
                error::guard(strings::toNumber<std::uint64_t>(tokens[1]))
            };
        })
    );

    setNumber(status.pendingSignals, "SigPnd", 16);
    setNumber(status.blockedSignals, "SigBlk", 16);
    setNumber(status.ignoredSignals, "SigIgn", 16);
    setNumber(status.caughtSignals, "SigCgt", 16);
    setNumber(status.inheritableCapabilities, "CapInh", 16);
    setNumber(status.permittedCapabilities, "CapPrm", 16);
    setNumber(status.effectiveCapabilities, "CapEff", 16);
    setNumber(status.boundingCapabilities, "CapBnd", 16);
    setNumber(status.ambientCapabilities, "CapAmb", 16);
    setNumber(status.noNewPrivileges, "NoNewPrivs");
    setNumber(status.seccompMode, "Seccomp");

    set(status.speculationStoreBypass, take("Speculation_Store_Bypass"));

    set(status.allowedCPUs, take("Cpus_allowed").transform(parseAllowed));
    set(status.allowedCPUList, take("Cpus_allowed_list").transform(parseAllowedList));
    set(status.allowedMemoryNodes, take("Mems_allowed").transform(parseAllowed));
    set(status.allowedMemoryNodeList, take("Mems_allowed_list").transform(parseAllowedList));

    setNumber(status.voluntaryContextSwitches, "voluntary_ctxt_switches");
    setNumber(status.nonVoluntaryContextSwitches, "nonvoluntary_ctxt_switches");

    set(status.coreDumping, take("CoreDumping").transform([](const auto &value) { return value == "1"; }));
    set(status.thpEnabled, take("THP_enabled").transform([](const auto &value) { return value == "1"; }));

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
                throw error::StacktraceError<std::system_error>{errno, std::system_category()};

            break;
        }

        if (e->d_name == "."sv || e->d_name == ".."sv)
            continue;

        tasks.push_back(error::guard(strings::toNumber<pid_t>(e->d_name)));
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
            throw error::StacktraceError<std::runtime_error>{
                fmt::format(
                    "Malformed /proc/{}/maps: expected >= {} fields, got {}",
                    mPID,
                    MappingBasicFields,
                    fields.size()
                )
            };

        const auto tokens = strings::split(fields[0], "-", 1);

        if (tokens.size() != 2)
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed address range in /proc/{}/maps: '{}'", mPID, fields[0])
            };

        MemoryMapping memoryMapping;

        memoryMapping.start = error::guard(strings::toNumber<std::uint64_t>(tokens[0], 16));
        memoryMapping.end = error::guard(strings::toNumber<std::uint64_t>(tokens[1], 16));
        memoryMapping.offset = error::guard(strings::toNumber<std::uint64_t>(fields[2], 16));
        memoryMapping.inode = error::guard(strings::toNumber<std::uint64_t>(fields[4]));
        memoryMapping.device = fields[3];

        if (fields.size() > MappingBasicFields)
            memoryMapping.pathname = fields[5];

        const auto &permissions = fields[1];

        if (permissions.length() < MappingPermissionsLength)
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed permissions in /proc/{}/maps: '{}'", mPID, fields[1])
            };

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
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Malformed line in /proc/{}/io: missing ':' separator", mPID)
            };

        map.emplace(std::move(tokens[0]), strings::trim(tokens[1]));
    }

    const auto set = [&]<typename T>(T &var, const std::string &key) {
        const auto it = map.find(key);

        if (it == map.end())
            throw error::StacktraceError<std::runtime_error>{
                fmt::format("Missing required field '{}' in /proc/{}/io", key, mPID)
            };

        var = error::guard(strings::toNumber<T>(it->second));
    };

    IOStat stat;

    set(stat.readCharacters, "rchar");
    set(stat.writeCharacters, "wchar");
    set(stat.readSyscalls, "syscr");
    set(stat.writeSyscalls, "syscw");
    set(stat.readBytes, "read_bytes");
    set(stat.writeBytes, "write_bytes");
    set(stat.cancelledWriteBytes, "cancelled_write_bytes");

    return stat;
}

zero::os::linux::procfs::process::Process zero::os::linux::procfs::process::self() {
    return error::guard(open(getpid()));
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

std::list<pid_t> zero::os::linux::procfs::process::all() {
    std::list<pid_t> ids;
    auto iterator = error::guard(filesystem::readDirectory("/proc"));

    while (const auto entry = error::guard(iterator.next())) {
        if (!error::guard(entry->isDirectory()))
            continue;

        const auto id = strings::toNumber<pid_t>(entry->path().filename().native());

        if (!id)
            continue;

        ids.push_back(*id);
    }

    return ids;
}

Z_DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::linux::procfs::process::Process::Error)
