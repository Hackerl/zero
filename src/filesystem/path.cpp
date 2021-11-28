#include <zero/filesystem/path.h>

#ifdef _WIN32
#include <Shlwapi.h>
#elif __linux__
#include <sys/stat.h>
#include <memory>
#include <climits>
#include <libgen.h>
#include <unistd.h>
#include <cstring>
#endif

std::string zero::filesystem::path::getApplicationDirectory() {
    return getDirectoryName(getApplicationPath());
}

std::string zero::filesystem::path::getApplicationName() {
    return getBaseName(getApplicationPath());
}

bool zero::filesystem::path::isDirectory(const std::string &path) {
    struct stat sb = {};

    if (stat(path.c_str(), &sb) != 0)
        return false;

    return (sb.st_mode & S_IFMT) == S_IFDIR;
}

bool zero::filesystem::path::isRegularFile(const std::string &path) {
    struct stat sb = {};

    if (stat(path.c_str(), &sb) != 0)
        return false;

    return (sb.st_mode & S_IFMT) == S_IFREG;
}

std::string zero::filesystem::path::join(const std::string &path) {
    return path;
}

#ifdef _WIN32
std::string zero::filesystem::path::getApplicationPath() {
    char buffer[MAX_PATH] = {};

    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    if (length == 0 || length == MAX_PATH)
        return "";

    return buffer;
}

std::string zero::filesystem::path::getAbsolutePath(const std::string &path) {
    char buffer[MAX_PATH] = {};

    if (!PathCanonicalizeA(buffer, path.c_str()))
        return "";

    return buffer;
}

std::string zero::filesystem::path::getBaseName(const std::string &path) {
    char buffer[MAX_PATH] = {};

    strncpy_s(buffer, path.c_str(), MAX_PATH - 1);
    PathStripPathA(buffer);

    return buffer;
}

std::string zero::filesystem::path::getDirectoryName(const std::string &path) {
    char buffer[MAX_PATH] = {};

    strncpy_s(buffer, path.c_str(), MAX_PATH - 1);

    if (!PathRemoveFileSpecA(buffer))
        return path;

    return buffer;
}

std::string zero::filesystem::path::getTemporaryDirectory() {
    char buffer[MAX_PATH + 1] = {};

    if (!GetTempPathA(MAX_PATH + 1, buffer))
        return "";

    return buffer;
}

#elif __linux__
std::string zero::filesystem::path::getFileDescriptorPath(int fd) {
    char buffer[PATH_MAX + 1] = {};

    if (readlink(join("/proc/self/fd", std::to_string(fd)).c_str(), buffer, PATH_MAX) == -1)
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getApplicationPath() {
    char buffer[PATH_MAX + 1] = {};

    if (readlink("/proc/self/exe", buffer, PATH_MAX) == -1)
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getAbsolutePath(const std::string &path) {
    char buffer[PATH_MAX] = {};

    if (!realpath(path.c_str(), buffer))
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getBaseName(const std::string &path) {
    return basename(std::unique_ptr<char, decltype(free) *>(strdup(path.c_str()), free).get());
}

std::string zero::filesystem::path::getDirectoryName(const std::string &path) {
    return dirname(std::unique_ptr<char, decltype(free) *>(strdup(path.c_str()), free).get());
}

std::string zero::filesystem::path::getTemporaryDirectory() {
    return P_tmpdir;
}

#endif
