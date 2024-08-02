#include <zero/filesystem/path.h>

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

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::getApplicationPath() {
#ifdef _WIN32
    std::array<WCHAR, MAX_PATH> buffer = {};

    if (const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), buffer.size());
        length == 0 || length == buffer.size())
        return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    return buffer.data();
#elif defined(__linux__)
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);

    if (ec)
        return tl::unexpected(ec);

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
        return tl::unexpected(ec);

    return path;
#else
#error "unsupported platform"
#endif
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::getApplicationDirectory() {
    return getApplicationPath().transform(&std::filesystem::path::parent_path);
}
