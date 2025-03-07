#include <catch_extensions.h>
#include <zero/os/resource.h>
#include <zero/filesystem/fs.h>
#include <zero/defer.h>

#ifdef _WIN32
#include <zero/os/windows/error.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <zero/os/unix/error.h>
#endif

TEST_CASE("operating system resource", "[os::resource]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

#ifdef _WIN32
    const auto handle = CreateFileA(
        path.string().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    DEFER(REQUIRE(zero::filesystem::remove(path)));

    const auto raw = handle;
#else
    const auto fd = zero::os::unix::expected([&] {
        return open(path.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
    });
    REQUIRE(fd);
    DEFER(REQUIRE(zero::filesystem::remove(path)));

    const auto raw = *fd;
#endif
    zero::os::Resource resource{raw};

    SECTION("get") {
        REQUIRE(resource.get() == raw);
    }

    SECTION("operator *") {
        REQUIRE(*resource == raw);
    }

    SECTION("valid") {
        REQUIRE(resource.valid());
    }

    SECTION("operator bool") {
        REQUIRE(resource);
    }

    SECTION("is inherited") {
        SECTION("inherited") {
            REQUIRE(resource.setInherited(true));
            REQUIRE(resource.isInherited() == true);
        }

        SECTION("not inherited") {
            REQUIRE(resource.isInherited() == false);
        }
    }

    SECTION("duplicate") {
        SECTION("inherited") {
            const auto duplicate = resource.duplicate(true);
            REQUIRE(duplicate);
            REQUIRE(duplicate->isInherited() == true);
        }

        SECTION("not inherited") {
            const auto duplicate = resource.duplicate(false);
            REQUIRE(duplicate);
            REQUIRE(duplicate->isInherited() == false);
        }
    }

    SECTION("setInherited") {
        SECTION("inherited") {
            REQUIRE(resource.setInherited(true));
            REQUIRE(resource.isInherited() == true);
        }

        REQUIRE(resource.setInherited(true));

        SECTION("not inherited") {
            REQUIRE(resource.setInherited(false));
            REQUIRE(resource.isInherited() == false);
        }
    }

    SECTION("release") {
        const auto native = resource.release();
        REQUIRE(native == raw);
        REQUIRE_FALSE(resource);
#ifdef _WIN32
        REQUIRE(zero::os::windows::expected([&] {
            return CloseHandle(native);
        }));
#else
        REQUIRE(zero::os::unix::expected([&] {
            return close(native);
        }));
#endif
    }

    SECTION("close") {
        REQUIRE(resource.close());
        REQUIRE_FALSE(resource);
    }
}
