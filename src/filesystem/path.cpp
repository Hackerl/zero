#include <filesystem/path.h>
#include <climits>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

std::string zero::filesystem::path::getFileDescriptorPath(int fd) {
    char buffer[PATH_MAX + 1] = {};

    if (readlink(join("/proc/self/fd", std::to_string(fd)).c_str(), buffer, PATH_MAX) == -1)
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getApplicationDirectory() {
    char buffer[PATH_MAX + 1] = {};

    if (readlink("/proc/self/exe", buffer, PATH_MAX) == -1)
        return "";

    return {dirname(buffer)};
}

std::string zero::filesystem::path::getApplicationPath() {
    char buffer[PATH_MAX + 1] = {};

    if (readlink("/proc/self/exe", buffer, PATH_MAX) == -1)
        return "";

    return {buffer};
}

std::string zero::filesystem::path::getApplicationName() {
    char buffer[PATH_MAX + 1] = {};

    if (readlink("/proc/self/exe", buffer, PATH_MAX) == -1)
        return "";

    return {basename(buffer)};
}

std::string zero::filesystem::path::getAbsolutePath(const std::string &path) {
    char buffer[PATH_MAX] = {};

    if (!realpath(path.c_str(), buffer))
        return "";

    return {buffer};
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
