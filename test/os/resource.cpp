#include <catch_extensions.h>
#include <zero/os/resource.h>
#include <zero/filesystem/fs.h>
#include <zero/defer.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>

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

    SECTION("is inheritable") {
        SECTION("inheritable") {
            REQUIRE(resource.setInheritable(true));
            REQUIRE(resource.isInheritable() == true);
        }

        SECTION("not inheritable") {
            REQUIRE(resource.isInheritable() == false);
        }
    }

    SECTION("duplicate") {
        SECTION("inheritable") {
            const auto duplicate = resource.duplicate(true);
            REQUIRE(duplicate);
            REQUIRE(duplicate->isInheritable() == true);
        }

        SECTION("not inheritable") {
            const auto duplicate = resource.duplicate(false);
            REQUIRE(duplicate);
            REQUIRE(duplicate->isInheritable() == false);
        }
    }

    SECTION("set inheritable") {
        SECTION("inheritable") {
            REQUIRE(resource.setInheritable(true));
            REQUIRE(resource.isInheritable() == true);
        }

        REQUIRE(resource.setInheritable(true));

        SECTION("not inheritable") {
            REQUIRE(resource.setInheritable(false));
            REQUIRE(resource.isInheritable() == false);
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

TEST_CASE("operating system i/o resource", "[os::resource]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto input = GENERATE(take(1, randomBytes(1, 102400)));

    REQUIRE(zero::filesystem::write(path, input));
    DEFER(REQUIRE(zero::filesystem::remove(path)));

#ifdef _WIN32
    const auto handle = CreateFileA(
        path.string().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    const auto raw = handle;
#else
    const auto fd = zero::os::unix::expected([&] {
        return open(path.c_str(), O_RDWR | O_CLOEXEC);
    });
    REQUIRE(fd);
    const auto raw = *fd;
#endif
    zero::os::IOResource resource{raw};

    SECTION("fd") {
        REQUIRE(resource.fd() == raw);
    }

    SECTION("valid") {
        REQUIRE(resource.valid());
    }

    SECTION("operator bool") {
        REQUIRE(resource);
    }

    SECTION("is inheritable") {
        SECTION("inheritable") {
            REQUIRE(resource.setInheritable(true));
            REQUIRE(resource.isInheritable() == true);
        }

        SECTION("not inheritable") {
            REQUIRE(resource.isInheritable() == false);
        }
    }

    SECTION("duplicate") {
        SECTION("inheritable") {
            const auto duplicate = resource.duplicate(true);
            REQUIRE(duplicate);
            REQUIRE(duplicate->isInheritable() == true);
        }

        SECTION("not inheritable") {
            const auto duplicate = resource.duplicate(false);
            REQUIRE(duplicate);
            REQUIRE(duplicate->isInheritable() == false);
        }
    }

    SECTION("set inheritable") {
        SECTION("inheritable") {
            REQUIRE(resource.setInheritable(true));
            REQUIRE(resource.isInheritable() == true);
        }

        REQUIRE(resource.setInheritable(true));

        SECTION("not inheritable") {
            REQUIRE(resource.setInheritable(false));
            REQUIRE(resource.isInheritable() == false);
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

    SECTION("read") {
        SECTION("normal") {
            std::vector<std::byte> content;
            content.resize(input.size());

            REQUIRE(resource.read(content) == input.size());
            REQUIRE_THAT(content, Catch::Matchers::RangeEquals(input));
        }

        SECTION("eof") {
            REQUIRE(resource.readAll());

            std::array<std::byte, 64> data{};
            REQUIRE(resource.read(data) == 0);
        }
    }

    SECTION("write") {
        const auto reversed = input | std::views::reverse | std::ranges::to<std::vector>();
        REQUIRE(resource.write(reversed) == reversed.size());

        const auto content = zero::filesystem::read(path);
        REQUIRE(content);
        REQUIRE_THAT(*content, Catch::Matchers::RangeEquals(reversed));
    }

    SECTION("position") {
        REQUIRE(resource.position() == 0);
        REQUIRE(resource.readAll());
        REQUIRE(resource.position() == input.size());
    }

    SECTION("length") {
        REQUIRE(resource.length() == input.size());
    }

    SECTION("rewind") {
        REQUIRE(resource.readAll());
        REQUIRE(resource.position() == input.size());
        REQUIRE(resource.rewind());
        REQUIRE(resource.position() == 0);
    }

    SECTION("seek") {
        const auto offset = GENERATE_REF(take(1, random<std::size_t>(0, input.size() - 1)));

        SECTION("begin") {
            REQUIRE(resource.seek(offset, zero::io::ISeekable::Whence::BEGIN) == offset);
        }

        SECTION("current") {
            REQUIRE(resource.seek(offset, zero::io::ISeekable::Whence::CURRENT) == offset);
        }

        SECTION("end") {
            REQUIRE(resource.seek(
                -(static_cast<std::int64_t>(input.size() - offset)),
                zero::io::ISeekable::Whence::END
            ) == offset);
        }

        const auto content = resource.readAll();
        REQUIRE(content);
        REQUIRE_THAT(
            *content,
            Catch::Matchers::RangeEquals(std::span{input.begin() + offset, input.end()})
        );
    }
}
