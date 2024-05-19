#ifndef ZERO_FILE_H
#define ZERO_FILE_H

#include <span>
#include <vector>
#include <expected>
#include <filesystem>

namespace zero::filesystem {
    std::expected<std::vector<std::byte>, std::error_code> read(const std::filesystem::path &path);
    std::expected<std::string, std::error_code> readString(const std::filesystem::path &path);

    std::expected<void, std::error_code> write(const std::filesystem::path &path, std::span<const std::byte> content);
    std::expected<void, std::error_code> writeString(const std::filesystem::path &path, std::string_view content);
}

#endif //ZERO_FILE_H
