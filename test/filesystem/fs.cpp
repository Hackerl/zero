#include <catch_extensions.h>
#include <zero/filesystem/fs.h>
#include <zero/strings/strings.h>
#include <zero/defer.h>
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
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file does not exist") {
        REQUIRE_ERROR(zero::filesystem::read(path), std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        const auto content = GENERATE(take(1, randomBytes(1, 102400)));
        REQUIRE(zero::filesystem::write(path, content));
        REQUIRE(zero::filesystem::read(path) == content);
        REQUIRE(zero::filesystem::remove(path));
    }
}

TEST_CASE("read string from file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file does not exist") {
        REQUIRE_ERROR(zero::filesystem::readString(path), std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        const auto content = GENERATE(take(1, randomString(1, 102400)));
        REQUIRE(zero::filesystem::write(path, content));
        REQUIRE(zero::filesystem::readString(path) == content);
        REQUIRE(zero::filesystem::remove(path));
    }
}

TEST_CASE("write bytes to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto content = GENERATE(take(1, randomBytes(1, 102400)));

    REQUIRE(zero::filesystem::write(path, content));
    Z_DEFER(REQUIRE(zero::filesystem::remove(path)));
    REQUIRE(zero::filesystem::read(path) == content);
}

TEST_CASE("write string to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto content = GENERATE(take(1, randomString(1, 102400)));

    REQUIRE(zero::filesystem::write(path, content));
    Z_DEFER(REQUIRE(zero::filesystem::remove(path)));
    REQUIRE(zero::filesystem::readString(path) == content);
}

TEST_CASE("copy file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto from = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto to = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));
    const auto content = GENERATE(take(1, randomBytes(1, 102400)));

    SECTION("source file does not exist") {
        REQUIRE_ERROR(zero::filesystem::copyFile(from, to), std::errc::no_such_file_or_directory);
    }

    SECTION("destination file does not exist") {
        REQUIRE(zero::filesystem::write(from, content));
        Z_DEFER(REQUIRE(zero::filesystem::remove(from)));

        REQUIRE(zero::filesystem::copyFile(from, to));
        Z_DEFER(REQUIRE(zero::filesystem::remove(to)));

        REQUIRE(zero::filesystem::read(to) == content);
    }

    SECTION("destination file exists") {
        REQUIRE(zero::filesystem::write(from, content));
        Z_DEFER(REQUIRE(zero::filesystem::remove(from)));

        REQUIRE(zero::filesystem::write(to, ""));
        Z_DEFER(REQUIRE(zero::filesystem::remove(to)));

        SECTION("default") {
            REQUIRE_ERROR(zero::filesystem::copyFile(from, to), std::errc::file_exists);
        }

        SECTION("overwrite") {
            REQUIRE(zero::filesystem::copyFile(from, to, std::filesystem::copy_options::overwrite_existing));
            REQUIRE(zero::filesystem::read(to) == content);
        }
    }
}

TEST_CASE("create directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        Z_DEFER(REQUIRE(zero::filesystem::remove(directory)));
        REQUIRE(zero::filesystem::exists(directory) == true);
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        Z_DEFER(REQUIRE(zero::filesystem::remove(directory)));
        REQUIRE_ERROR(zero::filesystem::createDirectory(directory), std::errc::file_exists);
    }
}

TEST_CASE("create directories", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        Z_DEFER(REQUIRE(zero::filesystem::removeAll(directory)));
        REQUIRE(zero::filesystem::exists(directory) == true);
        REQUIRE(zero::filesystem::exists(directory / "sub") == true);
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        Z_DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        REQUIRE(zero::filesystem::exists(directory) == true);
        REQUIRE(zero::filesystem::exists(directory / "sub") == true);
    }

    SECTION("parent directory exists") {
        REQUIRE(zero::filesystem::createDirectories(directory));
        Z_DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        REQUIRE(zero::filesystem::exists(directory) == true);
        REQUIRE(zero::filesystem::exists(directory / "sub") == true);
    }
}

TEST_CASE("remove", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::remove(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::write(path, ""));
            REQUIRE(zero::filesystem::remove(path));
            REQUIRE(zero::filesystem::exists(path) == false);
        }
    }

    SECTION("directory") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::remove(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::createDirectory(path));
            REQUIRE(zero::filesystem::remove(path));
            REQUIRE(zero::filesystem::exists(path) == false);
        }

        SECTION("not empty") {
            REQUIRE(zero::filesystem::createDirectories(path / "sub"));
            Z_DEFER(REQUIRE(zero::filesystem::removeAll(path)));
            REQUIRE_ERROR(zero::filesystem::remove(path), std::errc::directory_not_empty);
        }
    }
}

TEST_CASE("remove all", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("file") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::removeAll(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::write(path, ""));
            REQUIRE(zero::filesystem::removeAll(path) == 1);
            REQUIRE(zero::filesystem::exists(path) == false);
        }
    }

    SECTION("directory") {
        SECTION("does not exist") {
            REQUIRE_ERROR(zero::filesystem::removeAll(path), std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::createDirectories(path / "sub"));
            REQUIRE(zero::filesystem::removeAll(path) == 2);
            REQUIRE(zero::filesystem::exists(path) == false);
        }
    }
}

TEST_CASE("read directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE_ERROR(zero::filesystem::readDirectory(directory / "z"), std::errc::no_such_file_or_directory);
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        Z_DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

        const std::list files{directory / "a", directory / "b", directory / "c"};

        for (const auto &file: files) {
            REQUIRE(zero::filesystem::write(file, ""));
        }

        auto it = zero::filesystem::readDirectory(directory);
        REQUIRE(it);

        SECTION("prefix increment") {
            auto entry = **it;
            REQUIRE(entry);
            REQUIRE_THAT(files, Catch::Matchers::Contains(entry->path()));

            entry = *++*it;
            REQUIRE(entry);
            REQUIRE_THAT(files, Catch::Matchers::Contains(entry->path()));

            entry = *++*it;
            REQUIRE(entry);
            REQUIRE_THAT(files, Catch::Matchers::Contains(entry->path()));
            REQUIRE(++*it == end(*it));
        }

        SECTION("postfix increment") {
            auto entry = *(*it)++;
            REQUIRE(entry);
            REQUIRE_THAT(files, Catch::Matchers::Contains(entry->path()));

            entry = *(*it)++;
            REQUIRE(entry);
            REQUIRE_THAT(files, Catch::Matchers::Contains(entry->path()));

            entry = *(*it)++;
            REQUIRE(entry);
            REQUIRE_THAT(files, Catch::Matchers::Contains(entry->path()));
            REQUIRE(*it == end(*it));
        }

        SECTION("collect") {
            REQUIRE_THAT(
                *it
                | std::views::transform([](const auto &entry) {
                    REQUIRE(entry);
                    return entry->path();
                })
                | std::ranges::to<std::list>(),
                Catch::Matchers::UnorderedRangeEquals(files)
            );
        }
    }
}

TEST_CASE("walk directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / GENERATE(take(1, randomAlphanumericString(8, 64)));

    SECTION("directory does not exist") {
        REQUIRE_ERROR(zero::filesystem::walkDirectory(directory / "z"), std::errc::no_such_file_or_directory);
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        Z_DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

        const std::list files{directory / "a", directory / "b" / "c", directory / "d" / "e" / "f"};

        for (const auto &file: files) {
            REQUIRE(zero::filesystem::createDirectories(file.parent_path()));
            REQUIRE(zero::filesystem::write(file, ""));
        }

        auto it = zero::filesystem::walkDirectory(directory);
        REQUIRE(it);

#ifndef _WIN32
        SECTION("error while traversing") {
            REQUIRE(zero::filesystem::permissions(directory / "b", std::filesystem::perms::none));

            const auto entries = std::ranges::to<std::list>(*it);
            REQUIRE_FALSE(entries.back());
            REQUIRE(entries.back().error() == std::errc::permission_denied);

            REQUIRE(zero::filesystem::permissions(directory / "b", std::filesystem::perms::all));
        }
#endif

        SECTION("collect") {
            REQUIRE_THAT(
                *it
                | std::views::transform([](const auto &entry) {
                    REQUIRE(entry);
                    return entry->path();
                })
                | std::views::filter([](const auto &path) {
                    const auto result = zero::filesystem::isRegularFile(path);
                    REQUIRE(result);
                    return *result;
                })
                | std::ranges::to<std::list>(),
                Catch::Matchers::UnorderedRangeEquals(files)
            );
        }
    }
}
