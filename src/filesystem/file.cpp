#include <zero/filesystem/file.h>
#include <fstream>

tl::expected<std::vector<std::byte>, std::error_code> zero::filesystem::read(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);

    if (!stream.is_open())
        return tl::unexpected(std::error_code(errno, std::system_category()));

    tl::expected<std::vector<std::byte>, std::error_code> result;

    while (true) {
        char buffer[1024] = {};

        stream.read(buffer, sizeof(buffer));
        std::copy_n(reinterpret_cast<const std::byte *>(buffer), stream.gcount(), std::back_inserter(*result));

        if (stream.fail()) {
            if (!stream.eof()) {
                result = tl::unexpected(std::error_code(errno, std::system_category()));
                break;
            }

            break;
        }
    }

    return result;
}

tl::expected<std::string, std::error_code> zero::filesystem::readString(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);

    if (!stream.is_open())
        return tl::unexpected(std::error_code(errno, std::system_category()));

    tl::expected<std::string, std::error_code> result;

    while (true) {
        char buffer[1024] = {};

        stream.read(buffer, sizeof(buffer));
        result->append(buffer, stream.gcount());

        if (stream.fail()) {
            if (!stream.eof()) {
                result = tl::unexpected(std::error_code(errno, std::system_category()));
                break;
            }

            break;
        }
    }

    return result;
}

tl::expected<void, std::error_code>
zero::filesystem::write(const std::filesystem::path &path, const nonstd::span<const std::byte> content) {
    std::ofstream stream(path, std::ios::binary);

    if (!stream.is_open())
        return tl::unexpected(std::error_code(errno, std::system_category()));

    if (!stream.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size())))
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::writeString(const std::filesystem::path &path, std::string_view content) {
    return write(path, nonstd::as_bytes(nonstd::span{content.begin(), content.end()}));
}
