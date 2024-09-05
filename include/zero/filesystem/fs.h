#ifndef ZERO_FS_H
#define ZERO_FS_H

#include <span>
#include <vector>
#include <expected>
#include <filesystem>

namespace zero::filesystem {
    std::expected<std::filesystem::path, std::error_code> applicationPath();
    std::expected<std::filesystem::path, std::error_code> applicationDirectory();

    std::expected<std::vector<std::byte>, std::error_code> read(const std::filesystem::path &path);
    std::expected<std::string, std::error_code> readString(const std::filesystem::path &path);

    std::expected<void, std::error_code> write(const std::filesystem::path &path, std::span<const std::byte> content);
    std::expected<void, std::error_code> write(const std::filesystem::path &path, std::string_view content);
}

#endif //ZERO_FS_H
