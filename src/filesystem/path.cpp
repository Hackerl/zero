#include <zero/filesystem/path.h>

#ifdef _WIN32
#include <Shlwapi.h>
#elif __linux__
#include <glob.h>
#endif

std::filesystem::path zero::filesystem::getApplicationPath() {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};

    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    if (length == 0 || length == MAX_PATH)
        return "";

    return buffer;
#elif __linux__
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

std::filesystem::path zero::filesystem::getApplicationDirectory() {
    return getApplicationPath().parent_path();
}

std::optional<std::list<std::filesystem::path>> zero::filesystem::glob(const std::string &pattern) {
    std::filesystem::path directory = std::filesystem::path(pattern).parent_path();

    if (!std::filesystem::is_directory(directory))
        return std::nullopt;

    std::list<std::filesystem::path> paths;

#ifdef _WIN32
    WIN32_FIND_DATAA data = {};
    HANDLE handle = FindFirstFileA(pattern.c_str(), &data);

    if (handle == INVALID_HANDLE_VALUE)
        return std::nullopt;

    do {
        paths.push_back(directory / data.cFileName);
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
    paths.sort();
#elif __linux__
    glob_t g = {};

    if (glob(pattern.c_str(), 0, nullptr, &g) != 0) {
        globfree(&g);
        return std::nullopt;
    }

    for (int i = 0; i < g.gl_pathc; i++) {
        paths.emplace_back(g.gl_pathv[i]);
    }

    globfree(&g);
#endif

    return paths;
}
