#include <zero/filesystem/fs.h>
#include <algorithm>
#include <fstream>

#ifdef _WIN32
#include <array>
#include <windows.h>
#elif defined(__APPLE__)
#include <array>
#include <mach-o/dyld.h>
#include <sys/param.h>
#include <zero/expect.h>
#include <zero/os/unix/error.h>
#endif

std::expected<std::filesystem::path, std::error_code> zero::filesystem::applicationPath() {
#ifdef _WIN32
    std::array<WCHAR, MAX_PATH> buffer = {};

    if (const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), buffer.size());
        length == 0 || length == buffer.size())
        return std::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    return buffer.data();
#elif defined(__linux__)
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);

    if (ec)
        return std::unexpected(ec);

    return path;
#elif defined(__APPLE__)
    std::array<char, MAXPATHLEN> buffer = {};
    std::uint32_t size = buffer.size();

    EXPECT(os::unix::expected([&] {
        return _NSGetExecutablePath(buffer.data(), &size);
    }));

    std::error_code ec;
    auto path = std::filesystem::weakly_canonical(buffer.data(), ec);

    if (ec)
        return std::unexpected(ec);

    return path;
#else
#error "unsupported platform"
#endif
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::applicationDirectory() {
    return applicationPath().transform(&std::filesystem::path::parent_path);
}

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
zero::filesystem::write(const std::filesystem::path &path, const std::string_view content) {
    return write(path, std::as_bytes(std::span{content}));
}
