#ifndef ZERO_PATH_H
#define ZERO_PATH_H

#include <optional>
#include <filesystem>

namespace zero::filesystem {
    std::optional<std::filesystem::path> getApplicationPath();
    std::optional<std::filesystem::path> getApplicationDirectory();
}

#endif //ZERO_PATH_H
