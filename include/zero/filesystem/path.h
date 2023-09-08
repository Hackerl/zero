#ifndef ZERO_PATH_H
#define ZERO_PATH_H

#include <filesystem>
#include <tl/expected.hpp>

namespace zero::filesystem {
    tl::expected<std::filesystem::path, std::error_code> getApplicationPath();
    tl::expected<std::filesystem::path, std::error_code> getApplicationDirectory();
}

#endif //ZERO_PATH_H
