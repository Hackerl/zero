#include <zero/os/os.h>

#ifdef _WIN32
#include <windows.h>
#include <Lmcons.h>
#elif __linux__
#include <unistd.h>
#include <climits>
#endif

std::optional<std::string> zero::os::hostname() {
#ifdef _WIN32
    char name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD length = sizeof(name);

    if (!GetComputerNameA(name, &length))
        return std::nullopt;

    return name;
#elif __linux__
    char name[HOST_NAME_MAX + 1] = {};

    if (gethostname(name, sizeof(name)) < 0)
        return std::nullopt;

    return name;
#endif
}

std::optional<std::string> zero::os::username() {
#ifdef _WIN32
    char name[UNLEN + 1] = {};
    DWORD length = sizeof(name);

    if (!GetUserNameA(name, &length))
        return std::nullopt;

    return name;
#elif __linux__
#if __ANDROID__ && __ANDROID_API__ < 28
    char *name = getlogin();

    if (!name)
        return std::nullopt;

    return name;
#else
    char name[LOGIN_NAME_MAX + 1] = {};

    if (getlogin_r(name, sizeof(name)) != 0)
        return std::nullopt;

    return name;
#endif
#endif
}
