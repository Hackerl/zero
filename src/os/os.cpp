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

tl::expected<std::string, std::error_code> zero::os::hostname() {
#ifdef _WIN32
    std::array<WCHAR, MAX_COMPUTERNAME_LENGTH + 1> buffer{};
    auto size = static_cast<DWORD>(buffer.size());

    EXPECT(nt::expected([&] {
        return GetComputerNameW(buffer.data(), &size);
    }));

    return strings::encode(buffer.data());
#elif defined(__linux__)
    std::array<char, HOST_NAME_MAX + 1> buffer{};

    EXPECT(unix::expected([&] {
        return gethostname(buffer.data(), buffer.size());
    }));

    return buffer.data();
#elif defined(__APPLE__)
    std::array<char, MAXHOSTNAMELEN> buffer{};

    EXPECT(unix::expected([&] {
        return gethostname(buffer.data(), buffer.size());
    }));

    return buffer.data();
#else
#error "unsupported platform"
#endif
}

tl::expected<std::string, std::error_code> zero::os::username() {
#ifdef _WIN32
    std::array<WCHAR, UNLEN + 1> buffer{};
    auto size = static_cast<DWORD>(buffer.size());

    EXPECT(nt::expected([&] {
        return GetUserNameW(buffer.data(), &size);
    }));

    return strings::encode(buffer.data());
#elif defined(__linux__) || __APPLE__
    const auto uid = geteuid();
    const auto max = sysconf(_SC_GETPW_R_SIZE_MAX);

    std::size_t size = max != -1 ? max : 1024;
    auto buffer = std::make_unique<char[]>(size);

    passwd pwd{};
    passwd *ptr{};

    while (true) {
        const auto n = getpwuid_r(uid, &pwd, buffer.get(), size, &ptr);

        if (n == 0) {
            if (!ptr)
                return tl::unexpected(GetUsernameError::NO_SUCH_ENTRY);

            return pwd.pw_name;
        }

        if (n != ERANGE)
            return tl::unexpected(std::error_code(n, std::system_category()));

        size *= 2;
        buffer = std::make_unique<char[]>(size);
    }
#else
#error "unsupported platform"
#endif
}

#ifndef _WIN32
DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::GetUsernameError)
#endif
