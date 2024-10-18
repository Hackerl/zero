#include <zero/env.h>
#include <zero/expect.h>

#ifdef _WIN32
#include <memory>
#include <cassert>
#include <zero/os/windows/error.h>
#include <zero/strings/strings.h>
#else
#include <cstdlib>
#include <zero/os/unix/error.h>
#endif

tl::expected<std::optional<std::string>, std::error_code> zero::env::get(const std::string &name) {
#ifdef _WIN32
    const auto str = strings::decode(name);
    EXPECT(str);

    DWORD size{1024};
    auto buffer = std::make_unique<WCHAR[]>(size);

    while (true) {
        const auto result = GetEnvironmentVariableW(str->c_str(), buffer.get(), size);
        assert(result != size);

        if (result > 0 && result < size)
            return strings::encode(buffer.get());

        if (result == 0) {
            if (const auto error = GetLastError(); error != ERROR_ENVVAR_NOT_FOUND)
                return tl::unexpected{std::error_code{static_cast<int>(error), std::system_category()}};

            return std::nullopt;
        }

        size = result;
        buffer = std::make_unique<WCHAR[]>(size);
    }
#else
    const auto *env = std::getenv(name.c_str());

    if (!env)
        return std::nullopt;

    return env;
#endif
}

tl::expected<void, std::error_code> zero::env::set(const std::string &name, const std::string &value) {
#ifdef _WIN32
    const auto k = strings::decode(name);
    EXPECT(k);

    const auto v = strings::decode(value);
    EXPECT(v);

    return os::windows::expected([&] {
        return SetEnvironmentVariableW(k->c_str(), v->c_str());
    });
#else
    EXPECT(os::unix::expected([&] {
        return setenv(name.c_str(), value.c_str(), 1);
    }));
    return {};
#endif
}

tl::expected<void, std::error_code> zero::env::unset(const std::string &name) {
#ifdef _WIN32
    return strings::decode(name).and_then([](const auto &str) {
        return os::windows::expected([&] {
            return SetEnvironmentVariableW(str.c_str(), nullptr);
        });
    });
#else
    EXPECT(os::unix::expected([&] {
        return unsetenv(name.c_str());
    }));
    return {};
#endif
}
