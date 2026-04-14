#include <catch_extensions.h>
#include <zero/filesystem.h>
#include <zero/error.h>
#include <zero/defer.h>
#include <zero/strings.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <list>

TEST_CASE("create path from UTF-8 string", "[filesystem]") {
    SECTION("valid") {
        REQUIRE_NOTHROW(zero::filesystem::path("你好"));
    }

#ifdef _WIN32
    SECTION("invalid") {
        REQUIRE_THROWS_MATCHES(
            zero::filesystem::path("\xc0\xaf"),
            std::system_error,
            Catch::Matchers::Predicate<std::system_error>([](const auto &error) {
                return error.code() == std::errc::illegal_byte_sequence;
            })
        );
    }
#endif
}

TEST_CASE("convert path to UTF-8 string", "[filesystem]") {
    std::setlocale(LC_ALL, "en_US.UTF-8");
    REQUIRE(zero::strings::decode(zero::filesystem::stringify(zero::filesystem::path("你好"))) == L"你好");
}

TEST_CASE("read bytes from file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto path = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file does not exist") {
        REQUIRE_ERROR(zero::filesystem::read(path), std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        const auto content = GENERATE(take(1, randomBytes(1, 102400)));
        zero::error::guard(zero::filesystem::write(path, content));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(path)));
        REQUIRE(zero::filesystem::read(path) == content);
    }
}

TEST_CASE("read string from file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto path = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file does not exist") {
        REQUIRE_ERROR(zero::filesystem::readString(path), std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        const auto content = GENERATE(take(1, randomString(1, 102400)));
        zero::error::guard(zero::filesystem::write(path, content));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(path)));
        REQUIRE(zero::filesystem::readString(path) == content);
    }
}

TEST_CASE("write bytes to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto path = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto content = GENERATE(take(1, randomBytes(1, 102400)));

    REQUIRE(zero::filesystem::write(path, content));
    Z_DEFER(zero::error::guard(zero::filesystem::remove(path)));
    REQUIRE(zero::error::guard(zero::filesystem::exists(path)));
    REQUIRE(zero::error::guard(zero::filesystem::read(path)) == content);
}

TEST_CASE("write string to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto path = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto content = GENERATE(take(1, randomString(1, 102400)));

    REQUIRE(zero::filesystem::write(path, content));
    Z_DEFER(zero::error::guard(zero::filesystem::remove(path)));
    REQUIRE(zero::error::guard(zero::filesystem::exists(path)));
    REQUIRE(zero::error::guard(zero::filesystem::readString(path)) == content);
}

TEST_CASE("copy file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto from = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto to = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto content = GENERATE(take(1, randomBytes(1, 102400)));

    SECTION("source file does not exist") {
        REQUIRE_ERROR(zero::filesystem::copyFile(from, to), std::errc::no_such_file_or_directory);
    }

    SECTION("destination file does not exist") {
        zero::error::guard(zero::filesystem::write(from, content));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(from)));

        REQUIRE(zero::filesystem::copyFile(from, to));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(to)));

        REQUIRE(zero::error::guard(zero::filesystem::exists(to)));
        REQUIRE(zero::error::guard(zero::filesystem::read(to)) == content);
    }

    SECTION("destination file exists") {
        zero::error::guard(zero::filesystem::write(from, content));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(from)));

        zero::error::guard(zero::filesystem::write(to, ""));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(to)));

        SECTION("default") {
            REQUIRE_ERROR(zero::filesystem::copyFile(from, to), std::errc::file_exists);
        }

        SECTION("overwrite") {
            REQUIRE(zero::filesystem::copyFile(from, to, std::filesystem::copy_options::overwrite_existing));
            REQUIRE(zero::error::guard(zero::filesystem::exists(to)));
            REQUIRE(zero::error::guard(zero::filesystem::read(to)) == content);
        }
    }
}

TEST_CASE("create directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto directory = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(directory)));
        REQUIRE(zero::error::guard(zero::filesystem::exists(directory)));
    }

    SECTION("directory exists") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::remove(directory)));
        REQUIRE_ERROR(zero::filesystem::createDirectory(directory), std::errc::file_exists);
    }
}

TEST_CASE("create directories", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto directory = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));
        REQUIRE(zero::error::guard(zero::filesystem::exists(directory / "sub")));
    }

    SECTION("directory exists") {
        zero::error::guard(zero::filesystem::createDirectories(directory / "sub"));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        REQUIRE(zero::error::guard(zero::filesystem::exists(directory / "sub")));
    }

    SECTION("parent directory exists") {
        zero::error::guard(zero::filesystem::createDirectories(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        REQUIRE(zero::error::guard(zero::filesystem::exists(directory / "sub")));
    }
}

TEST_CASE("remove", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto path = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::remove(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            zero::error::guard(zero::filesystem::write(path, ""));
            REQUIRE(zero::filesystem::remove(path));
            REQUIRE_FALSE(zero::error::guard(zero::filesystem::exists(path)));
        }
    }

    SECTION("directory") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::remove(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            zero::error::guard(zero::filesystem::createDirectory(path));
            REQUIRE(zero::filesystem::remove(path));
            REQUIRE_FALSE(zero::error::guard(zero::filesystem::exists(path)));
        }

        SECTION("not empty") {
            zero::error::guard(zero::filesystem::createDirectories(path / "sub"));
            Z_DEFER(zero::error::guard(zero::filesystem::removeAll(path)));
            REQUIRE_ERROR(zero::filesystem::remove(path), std::errc::directory_not_empty);
        }
    }
}

TEST_CASE("remove all", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto path = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::removeAll(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            zero::error::guard(zero::filesystem::write(path, ""));
            REQUIRE(zero::filesystem::removeAll(path) == 1);
            REQUIRE_FALSE(zero::error::guard(zero::filesystem::exists(path)));
        }
    }

    SECTION("directory") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::removeAll(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            zero::error::guard(zero::filesystem::createDirectories(path / "sub"));
            REQUIRE(zero::filesystem::removeAll(path) == 2);
            REQUIRE_FALSE(zero::error::guard(zero::filesystem::exists(path)));
        }
    }
}

TEST_CASE("read directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto directory = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE_ERROR(zero::filesystem::readDirectory(directory / "z"), std::errc::no_such_file_or_directory);
    }

    SECTION("directory exists") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        const std::list files{directory / "a", directory / "b", directory / "c"};

        for (const auto &file: files)
            zero::error::guard(zero::filesystem::write(file, ""));

        auto iterator = zero::filesystem::readDirectory(directory);
        REQUIRE(iterator);

        auto entry = iterator->next();
        REQUIRE(entry);
        REQUIRE(*entry);
        REQUIRE_THAT(files, Catch::Matchers::Contains(entry.value()->path()));

        entry = iterator->next();
        REQUIRE(entry);
        REQUIRE(*entry);
        REQUIRE_THAT(files, Catch::Matchers::Contains(entry.value()->path()));

        entry = iterator->next();
        REQUIRE(entry);
        REQUIRE(*entry);
        REQUIRE_THAT(files, Catch::Matchers::Contains(entry.value()->path()));

        entry = iterator->next();
        REQUIRE(entry);
        REQUIRE_FALSE(*entry);
    }
}

TEST_CASE("walk directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    const auto directory = temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE_ERROR(zero::filesystem::walkDirectory(directory / "z"), std::errc::no_such_file_or_directory);
    }

    SECTION("directory exists") {
        zero::error::guard(zero::filesystem::createDirectory(directory));
        Z_DEFER(zero::error::guard(zero::filesystem::removeAll(directory)));

        const std::list files{directory / "a", directory / "b" / "c", directory / "d" / "e" / "f"};

        for (const auto &file: files) {
            zero::error::guard(zero::filesystem::createDirectories(file.parent_path()));
            zero::error::guard(zero::filesystem::write(file, ""));
        }

        auto iterator = zero::filesystem::walkDirectory(directory);
        REQUIRE(iterator);

        std::list<std::filesystem::path> paths;

        while (true) {
            const auto entry = iterator->next();
            REQUIRE(entry);

            if (!*entry)
                break;

            if (!zero::error::guard(entry.value()->isRegularFile()))
                continue;

            paths.push_back(entry->value().path());
        }

        REQUIRE_THAT(paths, Catch::Matchers::UnorderedRangeEquals(files));
    }
}
