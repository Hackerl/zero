#include <zero/os/os.h>

#ifdef _WIN32
#include <windows.h>
#include <Lmcons.h>
#include <zero/strings/strings.h>
#elif __linux__
#include <unistd.h>
#include <climits>
#elif __APPLE__
#include <unistd.h>
#include <sys/param.h>
#endif

std::optional<std::string> zero::os::hostname() {
#ifdef _WIN32
    WCHAR name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD length = ARRAYSIZE(name);

    if (!GetComputerNameW(name, &length))
        return std::nullopt;

    return zero::strings::encode(name);
#elif __linux__
    char name[HOST_NAME_MAX + 1] = {};

    if (gethostname(name, sizeof(name)) < 0)
        return std::nullopt;

    return name;
#elif __APPLE__
    char name[MAXHOSTNAMELEN] = {};

    if (gethostname(name, sizeof(name)) < 0)
        return std::nullopt;

    return name;
#else
#error "unsupported platform"
#endif
}

std::optional<std::string> zero::os::username() {
#ifdef _WIN32
    WCHAR name[UNLEN + 1] = {};
    DWORD length = ARRAYSIZE(name);

    if (!GetUserNameW(name, &length))
        return std::nullopt;

    return zero::strings::encode(name);
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
#elif __APPLE__
    char name[MAXLOGNAME + 1] = {};

    if (getlogin_r(name, sizeof(name)) != 0)
        return std::nullopt;

    return name;
#else
#error "unsupported platform"
#endif
}
