#ifndef ZERO_FS_H
#define ZERO_FS_H

#include <vector>
#include <filesystem>
#include <tl/expected.hpp>
#include <nonstd/span.hpp>

namespace zero::filesystem {
    tl::expected<std::filesystem::path, std::error_code> applicationPath();
    tl::expected<std::filesystem::path, std::error_code> applicationDirectory();

    tl::expected<std::vector<std::byte>, std::error_code> read(const std::filesystem::path &path);
    tl::expected<std::string, std::error_code> readString(const std::filesystem::path &path);

    tl::expected<void, std::error_code> write(const std::filesystem::path &path, nonstd::span<const std::byte> content);
    tl::expected<void, std::error_code> write(const std::filesystem::path &path, std::string_view content);
}

#endif //ZERO_FS_H
