#include <zero/os/process.h>

#ifdef __linux__
#include <zero/strings/strings.h>
#include <fstream>
#include <algorithm>
#include <filesystem>

constexpr auto MAPPING_BASIC_FIELDS = 5;
constexpr auto MAPPING_PERMISSIONS_LENGTH = 4;

zero::os::process::Process::Process(pid_t pid) : mPID(pid) {

}

std::optional<zero::os::process::MemoryMapping> zero::os::process::Process::getImageBase(std::string_view path) const {
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

std::optional<zero::os::process::MemoryMapping> zero::os::process::Process::findMapping(uintptr_t address) const {
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

std::optional<std::string> zero::os::process::Process::comm() const {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(mPID) / "comm";
    std::ifstream stream(path);

    if (!stream.is_open())
        return std::nullopt;

    return std::string{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::optional<std::string> zero::os::process::Process::cmdline() const {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(mPID) / "cmdline";
    std::ifstream stream(path);

    if (!stream.is_open())
        return std::nullopt;

    return std::string{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::optional<std::map<std::string, std::string>> zero::os::process::Process::environ() const {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(mPID) / "environ";
    std::ifstream stream(path);

    if (!stream.is_open())
        return std::nullopt;

    std::vector<std::string> tokens = zero::strings::split(
            std::string{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()},
            {"\0", 1}
    );

    std::map<std::string, std::string> environ;

    for (const auto &token: tokens) {
        size_t pos = token.find('=');

        if (pos == std::string::npos)
            continue;

        environ[token.substr(0, pos)] = token.substr(pos + 1);
    }

    return environ;
}

std::optional<zero::os::process::Stat> zero::os::process::Process::stat() const {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(mPID) / "stat";
    std::ifstream stream(path);

    if (!stream.is_open())
        return std::nullopt;

    std::string str{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};

    size_t start = str.find('(');
    size_t end = str.find(')');

    if (start == std::string::npos || end == std::string::npos)
        return std::nullopt;

    Stat stat;

    stat.pid = zero::strings::toNumber<pid_t>(str.substr(0, start - 1)).value_or(-1);
    stat.comm = str.substr(start + 1, end - start - 1);

    std::vector<std::string> tokens = zero::strings::split(str.substr(end + 2), " ");

    size_t size = tokens.size();

    if (size < 35)
        return std::nullopt;

    auto it = tokens.begin();

    stat.state = it++->at(0);
    stat.ppid = *zero::strings::toNumber<pid_t>(*it++);
    stat.pgrp = *zero::strings::toNumber<pid_t>(*it++);
    stat.session = *zero::strings::toNumber<int>(*it++);
    stat.tty = *zero::strings::toNumber<int>(*it++);
    stat.tpgid = *zero::strings::toNumber<pid_t>(*it++);
    stat.flags = *zero::strings::toNumber<unsigned int>(*it++);
    stat.minFlt = *zero::strings::toNumber<unsigned long>(*it++);
    stat.cMinFlt = *zero::strings::toNumber<unsigned long>(*it++);
    stat.majFlt = *zero::strings::toNumber<unsigned long>(*it++);
    stat.cMajFlt = *zero::strings::toNumber<unsigned long>(*it++);
    stat.uTime = *zero::strings::toNumber<unsigned long>(*it++);
    stat.sTime = *zero::strings::toNumber<unsigned long>(*it++);
    stat.cuTime = *zero::strings::toNumber<unsigned long>(*it++);
    stat.csTime = *zero::strings::toNumber<unsigned long>(*it++);
    stat.priority = *zero::strings::toNumber<long>(*it++);
    stat.nice = *zero::strings::toNumber<long>(*it++);
    stat.numThreads = *zero::strings::toNumber<long>(*it++);
    stat.intervalValue = *zero::strings::toNumber<long>(*it++);
    stat.startTime = *zero::strings::toNumber<unsigned long long>(*it++);
    stat.vSize = *zero::strings::toNumber<unsigned long>(*it++);
    stat.rss = *zero::strings::toNumber<long>(*it++);
    stat.rssLimit = *zero::strings::toNumber<unsigned long>(*it++);
    stat.startCode = *zero::strings::toNumber<unsigned long>(*it++);
    stat.endCode = *zero::strings::toNumber<unsigned long>(*it++);
    stat.startStack = *zero::strings::toNumber<unsigned long>(*it++);
    stat.esp = *zero::strings::toNumber<unsigned long>(*it++);
    stat.eip = *zero::strings::toNumber<unsigned long>(*it++);
    stat.signal = *zero::strings::toNumber<unsigned long>(*it++);
    stat.blocked = *zero::strings::toNumber<unsigned long>(*it++);
    stat.sigIgnore = *zero::strings::toNumber<unsigned long>(*it++);
    stat.sigCatch = *zero::strings::toNumber<unsigned long>(*it++);
    stat.wChan = *zero::strings::toNumber<unsigned long>(*it++);
    stat.nSwap = *zero::strings::toNumber<unsigned long>(*it++);
    stat.cnSwap = *zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.exitSignal = zero::strings::toNumber<int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.processor = zero::strings::toNumber<int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.rtPriority = zero::strings::toNumber<unsigned int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.policy = zero::strings::toNumber<unsigned int>(*it++);

    if (it == tokens.end())
        return stat;

    stat.delayAcctBlkIOTicks = zero::strings::toNumber<unsigned long long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.guestTime = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.cGuestTime = zero::strings::toNumber<long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.startData = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.endData = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.startBrk = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.argStart = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.argEnd = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.envStart = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.envEnd = zero::strings::toNumber<unsigned long>(*it++);

    if (it == tokens.end())
        return stat;

    stat.exitCode = zero::strings::toNumber<int>(*it++);

    return stat;
}

std::optional<std::list<pid_t>> zero::os::process::Process::tasks() const {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(mPID) / "task";

    if (!std::filesystem::is_directory(path))
        return std::nullopt;

    std::list<pid_t> threads;

    for (const auto &entry: std::filesystem::directory_iterator(path)) {
        std::optional<pid_t> thread = strings::toNumber<pid_t>(entry.path().filename().string());

        if (!thread)
            return std::nullopt;

        threads.emplace_back(*thread);
    }

    return threads;
}

std::optional<std::list<zero::os::process::MemoryMapping>> zero::os::process::Process::maps() const {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(mPID) / "maps";
    std::ifstream stream(path);

    if (!stream.is_open())
        return std::nullopt;

    std::string line;
    std::list<MemoryMapping> memoryMappings;

    while (std::getline(stream, line)) {
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
#endif
