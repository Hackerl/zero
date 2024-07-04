#include <zero/filesystem/file.h>
#include <algorithm>
#include <fstream>
#include <array>

std::expected<std::vector<std::byte>, std::error_code> zero::filesystem::read(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);

    if (!stream.is_open())
        return std::unexpected(std::error_code(errno, std::generic_category()));

    std::expected<std::vector<std::byte>, std::error_code> result;

    while (true) {
        std::array<char, 1024> buffer = {};

        stream.read(buffer.data(), buffer.size());
        std::copy_n(reinterpret_cast<const std::byte *>(buffer.data()), stream.gcount(), std::back_inserter(*result));

        if (stream.fail()) {
            if (!stream.eof()) {
                result = std::unexpected(std::error_code(errno, std::generic_category()));
                break;
            }

            break;
        }
    }

    return result;
}

std::expected<std::string, std::error_code> zero::filesystem::readString(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);

    if (!stream.is_open())
        return std::unexpected(std::error_code(errno, std::generic_category()));

    std::expected<std::string, std::error_code> result;

    while (true) {
        std::array<char, 1024> buffer = {};

        stream.read(buffer.data(), buffer.size());
        result->append(buffer.data(), stream.gcount());

        if (stream.fail()) {
            if (!stream.eof()) {
                result = std::unexpected(std::error_code(errno, std::generic_category()));
                break;
            }

            break;
        }
    }

    return result;
}

std::expected<void, std::error_code>
zero::filesystem::write(const std::filesystem::path &path, const std::span<const std::byte> content) {
    std::ofstream stream(path, std::ios::binary);

    if (!stream.is_open())
        return std::unexpected(std::error_code(errno, std::generic_category()));

    if (!stream.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size())))
        return std::unexpected(std::error_code(errno, std::generic_category()));

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::writeString(const std::filesystem::path &path, const std::string_view content) {
    return write(path, std::as_bytes(std::span{content}));
}
