#include <filesystem/path.h>
#include <climits>
#include <libgen.h>
#include <unistd.h>

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

std::string zero::filesystem::path::getAbsolutePath(const char *path) {
    char buffer[PATH_MAX] = {};

    if (!realpath(path, buffer))
        return "";

    return {buffer};
}

std::string zero::filesystem::path::join(const std::string &path) {
    return path;
}
