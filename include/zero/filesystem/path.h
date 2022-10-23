#ifndef ZERO_PATH_H
#define ZERO_PATH_H

#include <list>
#include <filesystem>
#include <optional>
#include <string>

namespace zero::filesystem {
    std::optional<std::filesystem::path> getApplicationPath();
    std::optional<std::filesystem::path> getApplicationDirectory();
    std::optional<std::list<std::filesystem::path>> glob(const std::string &pattern);
}

#endif //ZERO_PATH_H
