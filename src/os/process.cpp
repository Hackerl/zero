#include <zero/os/process.h>

#ifdef __linux__
#include <zero/strings/strings.h>
#include <fstream>
#include <algorithm>
#include <filesystem>

constexpr auto MAPPING_BASIC_FIELDS = 5;
constexpr auto MAPPING_PERMISSIONS_LENGTH = 4;

std::optional<zero::os::process::ProcessMapping> zero::os::process::getImageBase(pid_t pid, std::string_view path) {
    std::optional<std::list<ProcessMapping>> processMappings = getProcessMappings(pid);

    if (!processMappings)
        return std::nullopt;

    auto it = std::find_if(processMappings->begin(), processMappings->end(), [=](const auto &m) {
        return m.pathname.find(path) != std::string::npos;
    });

    if (it == processMappings->end())
        return std::nullopt;

    return *it;
}

std::optional<zero::os::process::ProcessMapping> zero::os::process::getAddressMapping(pid_t pid, uintptr_t address) {
    std::optional<std::list<ProcessMapping>> processMappings = getProcessMappings(pid);

    if (!processMappings)
        return std::nullopt;

    auto it = std::find_if(processMappings->begin(), processMappings->end(), [=](const auto &m) {
        return m.start <= address && address < m.end;
    });

    if (it == processMappings->end())
        return std::nullopt;

    return *it;
}

std::optional<std::list<zero::os::process::ProcessMapping>> zero::os::process::getProcessMappings(pid_t pid) {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(pid) / "maps";
    std::ifstream stream(path);

    if (!stream.is_open())
        return std::nullopt;

    std::string line;
    std::list<ProcessMapping> processMappings;

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

        ProcessMapping processMapping = {};

        processMapping.start = *start;
        processMapping.end = *end;
        processMapping.offset = *offset;
        processMapping.inode = *inode;
        processMapping.device = fields[3];

        if (fields.size() > MAPPING_BASIC_FIELDS)
            processMapping.pathname = fields[5];

        std::string permissions = fields[1];

        if (permissions.length() < MAPPING_PERMISSIONS_LENGTH)
            return std::nullopt;

        if (permissions.at(0) == 'r')
            processMapping.permissions |= READ;

        if (permissions.at(1) == 'w')
            processMapping.permissions |= WRITE;

        if (permissions.at(2) == 'x')
            processMapping.permissions |= EXECUTE;

        if (permissions.at(3) == 's')
            processMapping.permissions |= SHARED;

        if (permissions.at(3) == 'p')
            processMapping.permissions |= PRIVATE;

        processMappings.push_back(processMapping);
    }

    return processMappings;
}

std::optional<std::list<pid_t>> zero::os::process::getThreads(pid_t pid) {
    std::filesystem::path path = std::filesystem::path("/proc") / std::to_string(pid) / "task";

    if (!std::filesystem::is_directory(path))
        return std::nullopt;

    std::list<pid_t> threads;

    for (const auto &entry : std::filesystem::directory_iterator(path)) {
        std::optional<pid_t> thread = strings::toNumber<pid_t>(entry.path().filename().string());

        if (!thread)
            return std::nullopt;

        threads.emplace_back(*thread);
    }

    return threads;
}
#endif
