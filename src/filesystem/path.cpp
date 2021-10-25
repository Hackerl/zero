#include <zero/filesystem/path.h>
#include <climits>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>
#include <memory>
#include <cstring>

std::string zero::filesystem::path::getFileDescriptorPath(int fd) {
    char buffer[PATH_MAX + 1] = {};

    if (readlink(join("/proc/self/fd", std::to_string(fd)).c_str(), buffer, PATH_MAX) == -1)
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getApplicationDirectory() {
    return getDirectoryName(getApplicationPath());
}

std::string zero::filesystem::path::getApplicationPath() {
    char buffer[PATH_MAX + 1] = {};

    if (readlink("/proc/self/exe", buffer, PATH_MAX) == -1)
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getApplicationName() {
    return getBaseName(getApplicationPath());
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

bool zero::filesystem::path::isDirectory(const std::string &path) {
    struct stat sb = {};

    if (stat(path.c_str(), &sb) != 0)
        return false;

    return S_ISDIR(sb.st_mode);
}

bool zero::filesystem::path::isRegularFile(const std::string &path) {
    struct stat sb = {};

    if (stat(path.c_str(), &sb) != 0)
        return false;

    return S_ISREG(sb.st_mode);
}

std::string zero::filesystem::path::join(const std::string &path) {
    return path;
}
