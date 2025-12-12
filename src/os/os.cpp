#include <zero/os/os.h>
#include <zero/expect.h>
#include <array>

#ifdef _WIN32
#include <windows.h>
#include <Lmcons.h>
#include <zero/os/windows/error.h>
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
    std::array<WCHAR, MAX_COMPUTERNAME_LENGTH + 1> buffer{};
    auto size = static_cast<DWORD>(buffer.size());

    Z_EXPECT(windows::expected([&] {
        return GetComputerNameW(buffer.data(), &size);
    }));

    return strings::encode(buffer.data());
#elif defined(__linux__)
    std::array<char, HOST_NAME_MAX + 1> buffer{};

    Z_EXPECT(unix::expected([&] {
        return gethostname(buffer.data(), buffer.size());
    }));

    return buffer.data();
#elif defined(__APPLE__)
    std::array<char, MAXHOSTNAMELEN> buffer{};

    Z_EXPECT(unix::expected([&] {
        return gethostname(buffer.data(), buffer.size());
    }));

    return buffer.data();
#else
#error "unsupported platform"
#endif
}

std::expected<std::string, std::error_code> zero::os::username() {
#ifdef _WIN32
    std::array<WCHAR, UNLEN + 1> buffer{};
    auto size = static_cast<DWORD>(buffer.size());

    Z_EXPECT(windows::expected([&] {
        return GetUserNameW(buffer.data(), &size);
    }));

    return strings::encode(buffer.data());
#elif defined(__linux__) || __APPLE__
    const auto uid = geteuid();
    const auto max = unix::expected([] {
        return sysconf(_SC_GETPW_R_SIZE_MAX);
    }).transform([](const auto &value) {
        return static_cast<std::size_t>(value);
    });

    auto size = max.value_or(1024);
    auto buffer = std::make_unique<char[]>(size);

    passwd pwd{};
    passwd *ptr{};

    while (true) {
        const auto n = getpwuid_r(uid, &pwd, buffer.get(), size, &ptr);

        if (n == 0) {
            if (!ptr)
                return std::unexpected{GetUsernameError::NO_SUCH_ENTRY};

            return pwd.pw_name;
        }

        if (n != ERANGE)
            return std::unexpected{std::error_code{n, std::system_category()}};

        size *= 2;
        buffer = std::make_unique<char[]>(size);
    }
#else
#error "unsupported platform"
#endif
}

std::expected<std::pair<zero::os::IOResource, zero::os::IOResource>, std::error_code>
zero::os::pipe() {
#ifdef _WIN32
    static std::atomic<std::size_t> number;
    const auto name = fmt::format(R"(\\?\pipe\zero\anonymous\{}-{})", GetCurrentProcessId(), number++);

    auto handle = CreateNamedPipeA(
        name.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,
        0,
        0,
        0,
        nullptr
    );

    if (handle == INVALID_HANDLE_VALUE)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    IOResource first{handle};

    handle = CreateFileA(
        name.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (handle == INVALID_HANDLE_VALUE)
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return std::pair{std::move(first), IOResource{handle}};
#else
    std::array<int, 2> fds{};

    Z_EXPECT(unix::expected([&] {
        return ::pipe(fds.data());
    }));

    return std::pair{IOResource{fds[0]}, IOResource{fds[1]}};
#endif
}

#ifndef _WIN32
Z_DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::GetUsernameError)
#endif
