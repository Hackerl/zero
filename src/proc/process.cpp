#include <zero/proc/process.h>
#include <zero/strings/string.h>
#include <zero/filesystem/path.h>
#include <zero/filesystem/directory.h>
#include <fstream>
#include <algorithm>
#include <cstring>

constexpr auto PROCESS_MAPPING_FIELDS = 5;

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

bool zero::proc::getProcessMappings(pid_t pid, std::list<CProcessMapping> &processMappings) {
    std::string path = filesystem::path::join("/proc", std::to_string(pid), "maps");
    std::ifstream stream(path);

    if (!stream.is_open())
        return false;

    std::string line;

    while (std::getline(stream, line)) {
        std::vector<std::string> fields = strings::split(strings::trimExtraSpace(line), ' ');

        if (fields.size() < PROCESS_MAPPING_FIELDS)
            continue;

        std::vector<std::string> address = strings::split(fields[0], '-');

        if (address.size() != 2)
            continue;

        CProcessMapping processMapping = {};

        strings::toNumber(address[0], processMapping.start, 16);
        strings::toNumber(address[1], processMapping.end, 16);
        strings::toNumber(fields[2], processMapping.offset, 16);
        strings::toNumber(fields[4], processMapping.inode);

        processMapping.permissions = fields[1];
        processMapping.device = fields[3];

        if (fields.size() > PROCESS_MAPPING_FIELDS)
            processMapping.pathname = fields[5];

        processMappings.push_back(processMapping);
    }

    return true;
}

bool zero::proc::getThreads(pid_t pid, std::list<pid_t> &threads) {
    std::string path = filesystem::path::join("/proc", std::to_string(pid), "task");

    for (const auto &entry : filesystem::CDirectory({path, 1})) {
        int thread = 0;

        if (!strings::toNumber(zero::filesystem::path::getBaseName(entry.path), thread))
            return false;

        threads.emplace_back(thread);
    }

    return true;
}
