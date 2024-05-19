#ifndef ZERO_PATH_H
#define ZERO_PATH_H

#include <expected>
#include <filesystem>

namespace zero::filesystem {
    std::expected<std::filesystem::path, std::error_code> getApplicationPath();
    std::expected<std::filesystem::path, std::error_code> getApplicationDirectory();
}

#endif //ZERO_PATH_H
