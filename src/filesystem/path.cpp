#include <zero/filesystem/path.h>

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <sys/param.h>
#include <zero/expect.h>
#include <zero/os/unix/error.h>
#endif

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::getApplicationPath() {
#ifdef _WIN32
    WCHAR buffer[MAX_PATH] = {};

    if (const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH); length == 0 || length == MAX_PATH)
        return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

    return buffer;
#elif __linux__
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);

    if (ec)
        return tl::unexpected(ec);

    return path;
#elif __APPLE__
    char buffer[MAXPATHLEN];
    std::uint32_t size = sizeof(buffer);

    EXPECT(os::unix::expected([&] {
        return _NSGetExecutablePath(buffer, &size);
    }));

    std::error_code ec;
    auto path = std::filesystem::weakly_canonical(buffer, ec);

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
