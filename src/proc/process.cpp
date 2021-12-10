#include <zero/proc/process.h>

#ifdef __linux__
#include <zero/strings/string.h>
#include <zero/filesystem/path.h>
#include <zero/filesystem/directory.h>
#include <fstream>
#include <algorithm>

constexpr auto MAPPING_BASIC_FIELDS = 5;
constexpr auto MAPPING_PERMISSIONS_LENGTH = 4;

bool zero::proc::getImageBase(pid_t pid, const std::string &path, zero::proc::CProcessMapping &processMapping) {
    std::list<CProcessMapping> processMappings;

    if (!getProcessMappings(pid, processMappings))
        return false;

    auto it = std::find_if(processMappings.begin(), processMappings.end(), [=](const auto &m) {
        return m.pathname.find(path) != std::string::npos;
    });

    if (it == processMappings.end())
        return false;

    processMapping = *it;

    return true;
}

bool zero::proc::getAddressMapping(pid_t pid, uintptr_t address, zero::proc::CProcessMapping &processMapping) {
    std::list<CProcessMapping> processMappings;

    if (!getProcessMappings(pid, processMappings))
        return false;

    auto it = std::find_if(processMappings.begin(), processMappings.end(), [=](const auto &m) {
        return m.start <= address && address < m.end;
    });

    if (it == processMappings.end())
        return false;

    processMapping = *it;

    return true;
}

bool zero::proc::getProcessMappings(pid_t pid, std::list<CProcessMapping> &processMappings) {
    std::string path = filesystem::path::join("/proc", std::to_string(pid), "maps");
    std::ifstream stream(path);

    if (!stream.is_open())
        return false;

    std::string line;

    while (std::getline(stream, line)) {
        std::vector<std::string> fields = strings::split(strings::trimExtraSpace(line), ' ');

        if (fields.size() < MAPPING_BASIC_FIELDS)
            continue;

        std::vector<std::string> address = strings::split(fields[0], '-');

        if (address.size() != 2)
            continue;

        CProcessMapping processMapping = {};

        strings::toNumber(address[0], processMapping.start, 16);
        strings::toNumber(address[1], processMapping.end, 16);
        strings::toNumber(fields[2], processMapping.offset, 16);
        strings::toNumber(fields[4], processMapping.inode);

        processMapping.device = fields[3];

        if (fields.size() > MAPPING_BASIC_FIELDS)
            processMapping.pathname = fields[5];

        std::string permissions = fields[1];

        if (permissions.length() < MAPPING_PERMISSIONS_LENGTH)
            return false;

        if (permissions.at(0) == 'r')
            processMapping.permissions |= READ_PERMISSION;

        if (permissions.at(1) == 'w')
            processMapping.permissions |= WRITE_PERMISSION;

        if (permissions.at(2) == 'x')
            processMapping.permissions |= EXECUTE_PERMISSION;

        if (permissions.at(3) == 's')
            processMapping.permissions |= SHARED_PERMISSION;

        if (permissions.at(3) == 'p')
            processMapping.permissions |= PRIVATE_PERMISSION;

        processMappings.push_back(processMapping);
    }

    return true;
}

bool zero::proc::getThreads(pid_t pid, std::list<pid_t> &threads) {
    std::string path = filesystem::path::join("/proc", std::to_string(pid), "task");

    if (!filesystem::path::isDirectory(path))
        return false;

    for (const auto &entry : filesystem::CDirectory({path, 1})) {
        int thread = 0;

        if (!strings::toNumber(zero::filesystem::path::getBaseName(entry.path), thread))
            return false;

        threads.emplace_back(thread);
    }

    return true;
}
#endif
