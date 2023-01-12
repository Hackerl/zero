#include <zero/os/procfs.h>
#include <zero/strings/strings.h>
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>

constexpr auto STAT_BASIC_FIELDS = 37;
constexpr auto MAPPING_BASIC_FIELDS = 5;
constexpr auto MAPPING_PERMISSIONS_LENGTH = 4;

zero::os::procfs::Process::Process(int fd, pid_t pid) : mFD(fd), mPID(pid) {

}

zero::os::procfs::Process::Process(zero::os::procfs::Process &&rhs) noexcept: mFD(-1), mPID(0) {
    std::swap(mFD, rhs.mFD);
    std::swap(mPID, rhs.mPID);
}

zero::os::procfs::Process::~Process() {
    if (mFD < 0)
        return;

    close(mFD);
}

std::optional<std::string> zero::os::procfs::Process::readFile(const char *filename) const {
    int fd = openat(mFD, filename, O_RDONLY);

    if (fd < 0)
        return std::nullopt;

    std::optional<std::string> content = std::make_optional<std::string>();

    while (true) {
        char buffer[1024];

        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n <= 0) {
            if (n == -1)
                content.reset();

            break;
        }

        content->append(buffer, n);
    }

    close(fd);

    return content;
}

pid_t zero::os::procfs::Process::pid() const {
    return mPID;
}

std::optional<zero::os::procfs::MemoryMapping> zero::os::procfs::Process::getImageBase(std::string_view path) const {
    std::optional<std::list<MemoryMapping>> memoryMappings = maps();

    if (!memoryMappings)
        return std::nullopt;

    auto it = std::find_if(memoryMappings->begin(), memoryMappings->end(), [=](const auto &m) {
        return m.pathname.find(path) != std::string::npos;
    });

    if (it == memoryMappings->end())
        return std::nullopt;

    return *it;
}

std::optional<zero::os::procfs::MemoryMapping> zero::os::procfs::Process::findMapping(uintptr_t address) const {
    std::optional<std::list<MemoryMapping>> memoryMappings = maps();

    if (!memoryMappings)
        return std::nullopt;

    auto it = std::find_if(memoryMappings->begin(), memoryMappings->end(), [=](const auto &m) {
        return m.start <= address && address < m.end;
    });

    if (it == memoryMappings->end())
        return std::nullopt;

    return *it;
}

std::optional<std::filesystem::path> zero::os::procfs::Process::exe() const {
    char buffer[PATH_MAX + 1] = {};

    if (readlinkat(mFD, "exe", buffer, PATH_MAX) == -1)
        return std::nullopt;

    return buffer;
}

std::optional<std::filesystem::path> zero::os::procfs::Process::cwd() const {
    char buffer[PATH_MAX + 1] = {};

    if (readlinkat(mFD, "cwd", buffer, PATH_MAX) == -1)
        return std::nullopt;

    return buffer;
}

std::optional<std::string> zero::os::procfs::Process::comm() const {
    std::optional<std::string> content = readFile("comm");

    if (!content)
        return std::nullopt;

    return strings::trim(*content);
}

std::optional<std::vector<std::string>> zero::os::procfs::Process::cmdline() const {
    std::optional<std::string> content = readFile("cmdline");

    if (!content)
        return std::nullopt;

    std::vector<std::string> tokens = strings::split(*content, {"\0", 1});
    std::vector<std::string> cmdline;

    std::copy_if(
            tokens.begin(),
            tokens.end(),
            std::back_inserter(cmdline),
            [](const auto &token) {
                return !token.empty();
            }
    );

    return cmdline;
}

std::optional<std::map<std::string, std::string>> zero::os::procfs::Process::environ() const {
    std::optional<std::string> content = readFile("environ");

    if (!content)
        return std::nullopt;

    std::vector<std::string> tokens = strings::split(*content, {"\0", 1});
    std::map<std::string, std::string> environ;

    for (const auto &token: tokens) {
        if (token.empty())
            continue;

        size_t pos = token.find('=');

        if (pos == std::string::npos)
            continue;

        environ[token.substr(0, pos)] = token.substr(pos + 1);
    }

    return environ;
}

std::optional<zero::os::procfs::Stat> zero::os::procfs::Process::stat() const {
    std::optional<std::string> content = readFile("stat");

    if (!content)
        return std::nullopt;

    size_t start = content->find('(');
    size_t end = content->rfind(')');

    if (start == std::string::npos || end == std::string::npos)
        return std::nullopt;

    Stat stat;

    stat.pid = strings::toNumber<pid_t>(content->substr(0, start - 1)).value_or(-1);
    stat.comm = content->substr(start + 1, end - start - 1);

    std::vector<std::string> tokens = strings::split(content->substr(end + 2), " ");

    if (tokens.size() < STAT_BASIC_FIELDS - 2)
        return std::nullopt;

    auto it = tokens.begin();

    stat.state = it++->at(0);
    stat.ppid = *strings::toNumber<pid_t>(*it++);
    stat.pgrp = *strings::toNumber<pid_t>(*it++);
    stat.session = *strings::toNumber<int>(*it++);
    stat.tty = *strings::toNumber<int>(*it++);
    stat.tpgid = *strings::toNumber<pid_t>(*it++);
    stat.flags = *strings::toNumber<unsigned int>(*it++);
    stat.minFlt = *strings::toNumber<unsigned long>(*it++);
    stat.cMinFlt = *strings::toNumber<unsigned long>(*it++);
    stat.majFlt = *strings::toNumber<unsigned long>(*it++);
    stat.cMajFlt = *strings::toNumber<unsigned long>(*it++);
    stat.uTime = *strings::toNumber<unsigned long>(*it++);
    stat.sTime = *strings::toNumber<unsigned long>(*it++);
    stat.cuTime = *strings::toNumber<unsigned long>(*it++);
    stat.csTime = *strings::toNumber<unsigned long>(*it++);
    stat.priority = *strings::toNumber<long>(*it++);
    stat.nice = *strings::toNumber<long>(*it++);
    stat.numThreads = *strings::toNumber<long>(*it++);
    stat.intervalValue = *strings::toNumber<long>(*it++);
    stat.startTime = *strings::toNumber<unsigned long long>(*it++);
    stat.vSize = *strings::toNumber<unsigned long>(*it++);
    stat.rss = *strings::toNumber<long>(*it++);
    stat.rssLimit = *strings::toNumber<unsigned long>(*it++);
    stat.startCode = *strings::toNumber<unsigned long>(*it++);
    stat.endCode = *strings::toNumber<unsigned long>(*it++);
    stat.startStack = *strings::toNumber<unsigned long>(*it++);
    stat.esp = *strings::toNumber<unsigned long>(*it++);
    stat.eip = *strings::toNumber<unsigned long>(*it++);
    stat.signal = *strings::toNumber<unsigned long>(*it++);
    stat.blocked = *strings::toNumber<unsigned long>(*it++);
    stat.sigIgnore = *strings::toNumber<unsigned long>(*it++);
    stat.sigCatch = *strings::toNumber<unsigned long>(*it++);
    stat.wChan = *strings::toNumber<unsigned long>(*it++);
    stat.nSwap = *strings::toNumber<unsigned long>(*it++);
    stat.cnSwap = *strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.exitSignal = strings::toNumber<int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.processor = strings::toNumber<int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.rtPriority = strings::toNumber<unsigned int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.policy = strings::toNumber<unsigned int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.delayAcctBlkIOTicks = strings::toNumber<unsigned long long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.guestTime = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.cGuestTime = strings::toNumber<long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.startData = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.endData = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.startBrk = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.argStart = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.argEnd = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.envStart = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.envEnd = strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.exitCode = strings::toNumber<int>(*it++);

    return stat;
}

template<typename T>
std::optional<T> statusIntegerField(const std::map<std::string, std::string> &map, const char *key, int base = 10) {
    auto it = map.find(key);

    if (it == map.end())
        return std::nullopt;

    return zero::strings::toNumber<T>(it->second, base);
}

std::optional<zero::os::procfs::Status> zero::os::procfs::Process::status() const {
    std::optional<std::string> content = readFile("status");

    if (!content)
        return std::nullopt;

    std::map<std::string, std::string> map;

    for (const auto &line: strings::split(*content, "\n")) {
        if (line.empty())
            continue;

        std::vector<std::string> tokens = strings::split(line, ":");

        if (tokens.size() != 2)
            continue;

        map[tokens[0]] = strings::trim(tokens[1]);
    }

    Status status;

    status.name = map["Name"];
    status.umask = statusIntegerField<mode_t>(map, "Umask", 8);
    status.state = map["State"];
    status.tgid = *strings::toNumber<pid_t>(map["Tgid"]);
    status.ngid = statusIntegerField<pid_t>(map, "Ngid");
    status.pid = *strings::toNumber<pid_t>(map["Pid"]);
    status.ppid = *strings::toNumber<pid_t>(map["PPid"]);
    status.tracerPID = *strings::toNumber<pid_t>(map["TracerPid"]);

    std::vector<std::string> tokens = strings::split(map["Uid"]);

    if (tokens.size() != 4)
        return std::nullopt;

    for (size_t i = 0; i < 4; i++)
        status.uid[i] = *strings::toNumber<uid_t>(tokens[i]);

    tokens = strings::split(map["Gid"]);

    if (tokens.size() != 4)
        return std::nullopt;

    for (size_t i = 0; i < 4; i++)
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

    tokens = strings::split(map["SigQ"], "/");

    if (tokens.size() != 2)
        return std::nullopt;

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
        status.cpusAllowed = std::vector<unsigned int>();

        for (const auto &token: strings::split(it->second, ",")) {
            status.cpusAllowed->emplace_back(*strings::toNumber<unsigned int>(token, 16));
        }
    }

    it = map.find("Cpus_allowed_list");

    if (it != map.end()) {
        status.cpusAllowedList = std::vector<std::pair<unsigned int, unsigned int>>();

        for (const auto &token: strings::split(it->second, ",")) {
            if (token.find('-') == std::string::npos) {
                unsigned int n = *strings::toNumber<unsigned int>(token);
                status.cpusAllowedList->emplace_back(n, n);
                break;
            }

            tokens = strings::split(token, "-");

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
        status.memoryNodesAllowed = std::vector<unsigned int>();

        for (const auto &token: strings::split(it->second, ",")) {
            status.memoryNodesAllowed->emplace_back(*strings::toNumber<unsigned int>(token, 16));
        }
    }

    it = map.find("Mems_allowed_list");

    if (it != map.end()) {
        status.memoryNodesAllowedList = std::vector<std::pair<unsigned int, unsigned int>>();

        for (const auto &token: strings::split(it->second, ",")) {
            if (token.find('-') == std::string::npos) {
                unsigned int n = *strings::toNumber<unsigned int>(token);
                status.memoryNodesAllowedList->emplace_back(n, n);
                break;
            }

            tokens = strings::split(token, "-");

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

std::optional<std::list<pid_t>> zero::os::procfs::Process::tasks() const {
    int fd = openat(mFD, "task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);

    if (fd < 0)
        return std::nullopt;

    DIR *dir = fdopendir(fd);

    if (!dir) {
        close(fd);
        return std::nullopt;
    }

    std::optional<std::list<pid_t>> tasks = std::make_optional<std::list<pid_t>>();

    for (dirent *e = readdir(dir); e; e = readdir(dir)) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        std::optional<pid_t> tid = strings::toNumber<pid_t>(e->d_name);

        if (!tid) {
            tasks.reset();
            break;
        }

        tasks->push_back(*tid);
    }

    closedir(dir);

    return tasks;
}

std::optional<std::list<zero::os::procfs::MemoryMapping>> zero::os::procfs::Process::maps() const {
    std::optional<std::string> content = readFile("maps");

    if (!content)
        return std::nullopt;

    std::list<MemoryMapping> memoryMappings;

    for (const auto &line: strings::split(*content, "\n")) {
        std::vector<std::string> fields = strings::split(strings::trimExtraSpace(line), " ");

        if (fields.size() < MAPPING_BASIC_FIELDS)
            continue;

        std::vector<std::string> address = strings::split(fields[0], "-");

        if (address.size() != 2)
            continue;

        std::optional<uintptr_t> start = strings::toNumber<uintptr_t>(address[0], 16);
        std::optional<uintptr_t> end = strings::toNumber<uintptr_t>(address[1], 16);
        std::optional<off_t> offset = strings::toNumber<off_t>(fields[2], 16);
        std::optional<ino_t> inode = strings::toNumber<ino_t>(fields[4]);

        if (!start || !end || !offset || !inode)
            return std::nullopt;

        MemoryMapping memoryMapping;

        memoryMapping.start = *start;
        memoryMapping.end = *end;
        memoryMapping.offset = *offset;
        memoryMapping.inode = *inode;
        memoryMapping.device = fields[3];

        if (fields.size() > MAPPING_BASIC_FIELDS)
            memoryMapping.pathname = fields[5];

        std::string permissions = fields[1];

        if (permissions.length() < MAPPING_PERMISSIONS_LENGTH)
            return std::nullopt;

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

        memoryMappings.push_back(memoryMapping);
    }

    return memoryMappings;
}

std::optional<zero::os::procfs::Process> zero::os::procfs::openProcess(pid_t pid) {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(pid);

    std::error_code ec;

    if (!std::filesystem::is_directory(path, ec))
        return std::nullopt;

    int fd = open(path.string().c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC);

    if (fd < 0)
        return std::nullopt;

    return Process{fd, pid};
}
