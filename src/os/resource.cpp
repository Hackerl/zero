#include <zero/os/resource.h>
#include <zero/expect.h>
#include <utility>
#include <cassert>

#ifdef _WIN32
#include <zero/os/windows/error.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <zero/os/unix/error.h>
#endif

#ifdef _WIN32
#define INVALID_RESOURCE INVALID_HANDLE_VALUE
#else
constexpr auto INVALID_RESOURCE = -1;
#endif

namespace {
    bool valid(const zero::os::Resource::Native native) {
#ifdef _WIN32
        return !native || native == INVALID_RESOURCE;
#else
        return native < 0;
#endif
    }
}

zero::os::Resource::Resource(const Native native) : mNative{native} {
}

zero::os::Resource::Resource(Resource &&rhs) noexcept : mNative{std::exchange(rhs.mNative, INVALID_RESOURCE)} {
}

zero::os::Resource &zero::os::Resource::operator=(Resource &&rhs) noexcept {
    mNative = std::exchange(rhs.mNative, INVALID_RESOURCE);
    return *this;
}

zero::os::Resource::~Resource() {
    if (!valid())
        return;

    const auto result = close();
    assert(result);
}

zero::os::Resource::Native zero::os::Resource::get() const {
    return mNative;
}

zero::os::Resource::Native zero::os::Resource::operator*() const {
    return get();
}

bool zero::os::Resource::valid() const {
#ifdef _WIN32
    return mNative != nullptr && mNative != INVALID_RESOURCE;
#else
    return mNative >= 0;
#endif
}

zero::os::Resource::operator bool() const {
    return valid();
}

std::expected<bool, std::error_code> zero::os::Resource::isInherited() const {
#ifdef _WIN32
    DWORD flags{};

    EXPECT(windows::expected([&] {
        return GetHandleInformation(mNative, &flags);
    }));

    return flags & HANDLE_FLAG_INHERIT;
#else
    return unix::expected([this] {
        return fcntl(mNative, F_GETFD);
    }).transform([](const auto &flags) {
        return !(flags & FD_CLOEXEC);
    });
#endif
}

std::expected<zero::os::Resource, std::error_code> zero::os::Resource::duplicate(const bool inherited) const {
#ifdef _WIN32
    HANDLE handle{};

    EXPECT(windows::expected([&] {
        return DuplicateHandle(
            GetCurrentProcess(),
            mNative,
            GetCurrentProcess(),
            &handle,
            0,
            inherited,
            DUPLICATE_SAME_ACCESS
        );
    }));

    return Resource{handle};
#else
    auto resource = unix::expected([this] {
        return dup(mNative);
    }).transform([](const auto &fd) {
        return Resource{fd};
    });

    if (!inherited) {
        EXPECT(resource->setInherited(false));
    }

    return *std::move(resource);
#endif
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::Resource::setInherited(const bool inherited) {
#ifdef _WIN32
    return windows::expected([&] {
        return SetHandleInformation(mNative, HANDLE_FLAG_INHERIT, inherited ? HANDLE_FLAG_INHERIT : 0);
    });
#else
    auto flags = unix::expected([this] {
        return fcntl(mNative, F_GETFD);
    });
    EXPECT(flags);

    if (inherited)
        *flags &= ~FD_CLOEXEC;
    else
        *flags |= FD_CLOEXEC;

    EXPECT(unix::expected([&] {
        return fcntl(mNative, F_SETFD, *flags);
    }));

    return {};
#endif
}

zero::os::Resource::Native zero::os::Resource::release() {
    return std::exchange(mNative, INVALID_RESOURCE);
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::Resource::close() {
#ifdef _WIN32
    return windows::expected([this] {
        return CloseHandle(std::exchange(mNative, INVALID_RESOURCE));
    });
#else
    EXPECT(unix::ensure([this] {
        return ::close(std::exchange(mNative, INVALID_RESOURCE));
    }));
    return {};
#endif
}
