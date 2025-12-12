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

std::expected<bool, std::error_code> zero::os::Resource::isInheritable() const {
#ifdef _WIN32
    DWORD flags{};

    Z_EXPECT(windows::expected([&] {
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

std::expected<zero::os::Resource, std::error_code> zero::os::Resource::duplicate(const bool inheritable) const {
#ifdef _WIN32
    HANDLE handle{};

    Z_EXPECT(windows::expected([&] {
        return DuplicateHandle(
            GetCurrentProcess(),
            mNative,
            GetCurrentProcess(),
            &handle,
            0,
            inheritable,
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

    if (!inheritable) {
        Z_EXPECT(resource->setInheritable(false));
    }

    return *std::move(resource);
#endif
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::expected<void, std::error_code> zero::os::Resource::setInheritable(const bool inheritable) {
#ifdef _WIN32
    return windows::expected([&] {
        return SetHandleInformation(mNative, HANDLE_FLAG_INHERIT, inheritable ? HANDLE_FLAG_INHERIT : 0);
    });
#else
    auto flags = unix::expected([this] {
        return fcntl(mNative, F_GETFD);
    });
    Z_EXPECT(flags);

    if (inheritable)
        *flags &= ~FD_CLOEXEC;
    else
        *flags |= FD_CLOEXEC;

    Z_EXPECT(unix::expected([&] {
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
    Z_EXPECT(unix::ensure([this] {
        return ::close(std::exchange(mNative, INVALID_RESOURCE));
    }));
    return {};
#endif
}

zero::os::IOResource::IOResource(const Resource::Native native) : mResource{native} {
}

zero::os::IOResource::IOResource(Resource resource) : mResource{std::move(resource)} {
}

zero::io::FileDescriptor zero::os::IOResource::fd() const {
    return *mResource;
}

bool zero::os::IOResource::valid() const {
    return mResource.valid();
}

zero::os::IOResource::operator bool() const {
    return valid();
}

std::expected<bool, std::error_code> zero::os::IOResource::isInheritable() const {
    return mResource.isInheritable();
}

std::expected<zero::os::IOResource, std::error_code> zero::os::IOResource::duplicate(const bool inheritable) const {
    return mResource.duplicate(inheritable).transform([](Resource resource) {
        return IOResource{std::move(resource)};
    });
}

std::expected<std::size_t, std::error_code> zero::os::IOResource::read(const std::span<std::byte> data) {
#ifdef _WIN32
    DWORD n{};

    Z_EXPECT(windows::expected([&] {
        return ReadFile(*mResource, data.data(), static_cast<DWORD>(data.size()), &n, nullptr);
    }).or_else([](const auto &ec) -> std::expected<void, std::error_code> {
        if (ec != std::errc::broken_pipe)
            return std::unexpected{ec};

        return {};
    }));

    return n;
#else
    return unix::ensure([&] {
        return ::read(*mResource, data.data(), data.size());
    });
#endif
}

std::expected<std::size_t, std::error_code> zero::os::IOResource::write(const std::span<const std::byte> data) {
#ifdef _WIN32
    DWORD n{};
    Z_EXPECT(windows::expected([&] {
        return WriteFile(*mResource, data.data(), static_cast<DWORD>(data.size()), &n, nullptr);
    }));
    return n;
#else
    return unix::ensure([&] {
        return ::write(*mResource, data.data(), data.size());
    });
#endif
}

std::expected<std::uint64_t, std::error_code> zero::os::IOResource::seek(const std::int64_t offset, const Whence whence) {
#ifdef _WIN32
    LARGE_INTEGER pos{};

    Z_EXPECT(windows::expected([&] {
        return SetFilePointerEx(
            *mResource,
            LARGE_INTEGER{.QuadPart = offset},
            &pos,
            whence == Whence::BEGIN ? FILE_BEGIN : whence == Whence::CURRENT ? FILE_CURRENT : FILE_END
        );
    }));

    return pos.QuadPart;
#else
    const auto pos = unix::expected([&] {
#ifdef _LARGEFILE64_SOURCE
        return lseek64(
#else
        return lseek(
#endif
            *mResource,
            offset,
            whence == Whence::BEGIN ? SEEK_SET : whence == Whence::CURRENT ? SEEK_CUR : SEEK_END
        );
    });
    Z_EXPECT(pos);

    return *pos;
#endif
}

std::expected<void, std::error_code> zero::os::IOResource::close() {
    return mResource.close();
}

std::expected<void, std::error_code> zero::os::IOResource::setInheritable(const bool inheritable) {
    return mResource.setInheritable(inheritable);
}

zero::io::FileDescriptor zero::os::IOResource::release() {
    return mResource.release();
}
