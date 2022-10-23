#include <zero/os/os.h>

#ifdef _WIN32
#include <Windows.h>
#include <Lmcons.h>
#elif __linux__
#include <unistd.h>
#include <climits>
#endif

std::string zero::os::hostname() {
#ifdef _WIN32
    char name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD length = sizeof(name);

    if (!GetComputerNameA(name, &length))
        return "";

    return name;
#elif __linux__
    char name[HOST_NAME_MAX + 1] = {};

    if (gethostname(name, sizeof(name)) < 0)
        return "";

    return name;
#endif
}

std::string zero::os::username() {
#ifdef _WIN32
    char name[UNLEN + 1] = {};
    DWORD length = sizeof(name);

    if (!GetUserNameA(name, &length))
        return "";

    return name;
#elif __linux__
    char name[LOGIN_NAME_MAX + 1] = {};

    if (getlogin_r(name, sizeof(name)) != 0)
        return "";

    return name;
#endif
}
