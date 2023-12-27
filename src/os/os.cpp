#include <zero/os/os.h>

#ifdef _WIN32
#include <windows.h>
#include <Lmcons.h>
#include <zero/strings/strings.h>
#elif __linux__
#include <pwd.h>
#include <unistd.h>
#include <climits>
#include <memory>
#elif __APPLE__
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>
#endif

tl::expected<std::string, std::error_code> zero::os::hostname() {
#ifdef _WIN32
    WCHAR name[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD length = ARRAYSIZE(name);

    if (!GetComputerNameW(name, &length))
        return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    return strings::encode(name);
#elif __linux__
    char name[HOST_NAME_MAX + 1] = {};

    if (gethostname(name, sizeof(name)) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return name;
#elif __APPLE__
    char name[MAXHOSTNAMELEN] = {};

    if (gethostname(name, sizeof(name)) < 0)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    return name;
#else
#error "unsupported platform"
#endif
}

tl::expected<std::string, std::error_code> zero::os::username() {
#ifdef _WIN32
    WCHAR name[UNLEN + 1] = {};
    DWORD length = ARRAYSIZE(name);

    if (!GetUserNameW(name, &length))
        return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

    return strings::encode(name);
#elif __linux__ || __APPLE__
    const uid_t uid = geteuid();
    const long max = sysconf(_SC_GETPW_R_SIZE_MAX);

    std::size_t length = max != -1 ? max : 1024;
    auto buffer = std::make_unique<char[]>(length);

    passwd pwd = {};
    passwd *ptr = nullptr;

    tl::expected<std::string, std::error_code> result;

    while (true) {
        if (const int n = getpwuid_r(uid, &pwd, buffer.get(), length, &ptr); n != 0) {
            if (n == ERANGE) {
                length *= 2;
                buffer = std::make_unique<char[]>(length);
                continue;
            }

            result = tl::unexpected(std::error_code(n, std::system_category()));
            break;
        }

        if (!ptr)
            break;

        result = pwd.pw_name;
        break;
    }

    return result;
#else
#error "unsupported platform"
#endif
}
