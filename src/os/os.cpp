#include <zero/os/os.h>
#include <zero/expect.h>
#include <array>

#ifdef _WIN32
#include <windows.h>
#include <Lmcons.h>
#include <zero/os/nt/error.h>
#include <zero/strings/strings.h>
#elif defined(__linux__)
#include <pwd.h>
#include <memory>
#include <climits>
#include <unistd.h>
#include <zero/os/unix/error.h>
#elif defined(__APPLE__)
#include <pwd.h>
#include <memory>
#include <unistd.h>
#include <sys/param.h>
#include <zero/os/unix/error.h>
#endif

std::expected<std::string, std::error_code> zero::os::hostname() {
#ifdef _WIN32
    std::array<WCHAR, MAX_COMPUTERNAME_LENGTH + 1> buffer = {};
    DWORD length = buffer.size();

    EXPECT(nt::expected([&] {
        return GetComputerNameW(buffer.data(), &length);
    }));

    return strings::encode(buffer.data());
#elif defined(__linux__)
    std::array<char, HOST_NAME_MAX + 1> buffer = {};

    EXPECT(unix::expected([&] {
        return gethostname(buffer.data(), buffer.size());
    }));

    return buffer.data();
#elif defined(__APPLE__)
    std::array<char, MAXHOSTNAMELEN> buffer = {};

    EXPECT(unix::expected([&] {
        return gethostname(buffer.data(), buffer.size());
    }));

    return buffer.data();
#else
#error "unsupported platform"
#endif
}

std::expected<std::string, std::error_code> zero::os::username() {
#ifdef _WIN32
    std::array<WCHAR, UNLEN + 1> buffer = {};
    DWORD length = buffer.size();

    EXPECT(nt::expected([&] {
        return GetUserNameW(buffer.data(), &length);
    }));

    return strings::encode(buffer.data());
#elif defined(__linux__) || __APPLE__
    const uid_t uid = geteuid();
    const long max = sysconf(_SC_GETPW_R_SIZE_MAX);

    std::size_t length = max != -1 ? max : 1024;
    auto buffer = std::make_unique<char[]>(length);

    passwd pwd = {};
    passwd *ptr = nullptr;

    std::expected<std::string, std::error_code> result;

    while (true) {
        if (const int n = getpwuid_r(uid, &pwd, buffer.get(), length, &ptr); n != 0) {
            if (n == ERANGE) {
                length *= 2;
                buffer = std::make_unique<char[]>(length);
                continue;
            }

            result = std::unexpected(std::error_code(n, std::system_category()));
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
