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
#include <zero/filesystem/std.h>
#elif defined(__linux__)
#include <zero/filesystem/std.h>
#endif

std::expected<std::filesystem::path, std::error_code> zero::filesystem::applicationPath() {
#ifdef _WIN32
    std::array<WCHAR, MAX_PATH> buffer{};

    if (const auto length = GetModuleFileNameW(nullptr, buffer.data(), buffer.size());
        length == 0 || length == buffer.size())
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return buffer.data();
#elif defined(__linux__)
    return readSymlink("/proc/self/exe");
#elif defined(__APPLE__)
    std::array<char, MAXPATHLEN> buffer{};
    auto size = static_cast<std::uint32_t>(buffer.size());

    EXPECT(os::unix::expected([&] {
        return _NSGetExecutablePath(buffer.data(), &size);
    }));

    return weaklyCanonical(buffer.data());
#else
#error "unsupported platform"
#endif
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::applicationDirectory() {
    return applicationPath().transform(&std::filesystem::path::parent_path);
}

std::expected<std::vector<std::byte>, std::error_code> zero::filesystem::read(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};

    if (!stream.is_open())
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    std::vector<std::byte> content;

    while (true) {
        std::array<std::byte, 1024> buffer; // NOLINT(*-pro-type-member-init)

        stream.read(reinterpret_cast<char *>(buffer.data()), buffer.size());

        // waiting for libstdc++ to implement std::vector::append_range
        // content.append_range(std::span{buffer.data(), static_cast<std::size_t>(stream.gcount())});
        content.insert(content.end(), buffer.begin(), buffer.begin() + stream.gcount());

        if (stream.fail()) {
            if (!stream.eof())
                return std::unexpected{std::error_code{errno, std::generic_category()}};

            break;
        }
    }

    return content;
}

std::expected<std::string, std::error_code> zero::filesystem::readString(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};

    if (!stream.is_open())
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    std::string content;

    while (true) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        stream.read(buffer.data(), buffer.size());
        content.append(buffer.data(), stream.gcount());

        if (stream.fail()) {
            if (!stream.eof())
                return std::unexpected{std::error_code{errno, std::generic_category()}};

            break;
        }
    }

    return content;
}

std::expected<void, std::error_code>
zero::filesystem::write(const std::filesystem::path &path, const std::span<const std::byte> content) {
    std::ofstream stream{path, std::ios::binary};

    if (!stream.is_open())
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    if (!stream.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size())))
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::write(const std::filesystem::path &path, const std::string_view content) {
    return write(path, std::as_bytes(std::span{content}));
}
