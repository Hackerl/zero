#include <zero/env.h>
#include <zero/error.h>
#include <zero/strings.h>

#ifdef _WIN32
#include <memory>
#include <cassert>
#include <zero/os/windows/error.h>
#else
#include <cstdlib>
#include <zero/os/unix/error.h>
#ifndef __APPLE__
#include <unistd.h>
#else
extern char **environ;
#endif
#endif

std::optional<std::string> zero::env::get(const std::string &name) {
#ifdef _WIN32
    const auto str = error::guard(strings::decode(name));

    DWORD size{1024};

    while (true) {
        const auto buffer = std::make_unique<WCHAR[]>(size);

        const auto result = GetEnvironmentVariableW(str.c_str(), buffer.get(), size);
        assert(result != size);

        if (result > 0 && result < size)
            return error::guard(strings::encode(buffer.get()));

        if (result == 0) {
            if (const auto error = GetLastError(); error != ERROR_ENVVAR_NOT_FOUND)
                throw error::StacktraceError<std::system_error>{static_cast<int>(error), std::system_category()};

            return std::nullopt;
        }

        size = result;
    }
#else
    const auto *env = std::getenv(name.c_str());

    if (!env)
        return std::nullopt;

    return env;
#endif
}

void zero::env::set(const std::string &name, const std::string &value) {
#ifdef _WIN32
    error::guard(os::windows::expected([&] {
        return SetEnvironmentVariableW(
            error::guard(strings::decode(name)).c_str(),
            error::guard(strings::decode(value)).c_str()
        );
    }));
#else
    error::guard(os::unix::expected([&] {
        return setenv(name.c_str(), value.c_str(), 1);
    }));
#endif
}

void zero::env::unset(const std::string &name) {
#ifdef _WIN32
    error::guard(os::windows::expected([&] {
        return SetEnvironmentVariableW(error::guard(strings::decode(name)).c_str(), nullptr);
    }));
#else
    error::guard(os::unix::expected([&] {
        return unsetenv(name.c_str());
    }));
#endif
}

std::map<std::string, std::string> zero::env::list() {
#ifdef _WIN32
    const std::unique_ptr<WCHAR, decltype(&FreeEnvironmentStringsW)> ptr{
        GetEnvironmentStringsW(),
        FreeEnvironmentStringsW
    };

    if (!ptr)
        throw error::StacktraceError<std::system_error>{static_cast<int>(GetLastError()), std::system_category()};

    std::map<std::string, std::string> envs;

    for (const auto *env = ptr.get(); *env != L'\0'; env += std::wcslen(env) + 1) {
        auto tokens = strings::split(error::guard(strings::encode(env)), "=", 1);

        if (tokens.size() != 2)
            continue;

        if (tokens[0].empty())
            continue;

        envs.emplace(std::move(tokens[0]), std::move(tokens[1]));
    }

    return envs;
#else
    std::map<std::string, std::string> envs;

    for (const auto *ptr = environ; *ptr; ++ptr) {
        auto tokens = strings::split(*ptr, "=", 1);

        if (tokens.size() != 2)
            continue;

        envs.emplace(std::move(tokens[0]), std::move(tokens[1]));
    }

    return envs;
#endif
}
