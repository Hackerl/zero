#include <zero/filesystem/path.h>

#ifdef _WIN32
#include <Windows.h>
#endif

std::optional<std::filesystem::path> zero::filesystem::getApplicationPath() {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};

    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    if (length == 0 || length == MAX_PATH)
        return std::nullopt;

    return buffer;
#elif __linux__
    std::error_code ec;
    std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", ec);

    if (ec)
        return std::nullopt;

    return path;
#endif
}

std::optional<std::filesystem::path> zero::filesystem::getApplicationDirectory() {
    std::optional<std::filesystem::path> path = getApplicationPath();

    if (!path)
        return std::nullopt;

    return path->parent_path();
}