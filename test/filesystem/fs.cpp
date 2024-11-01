#include <zero/filesystem/fs.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <list>

constexpr std::string_view CONTENT = "hello world";

TEST_CASE("read bytes from file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-read-bytes";

    SECTION("file does not exist") {
        const auto content = zero::filesystem::read(path);
        REQUIRE_FALSE(content);
        REQUIRE(content.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        REQUIRE(zero::filesystem::write(path, std::as_bytes(std::span{CONTENT})));

        const auto content = zero::filesystem::read(path);
        REQUIRE(content);
        REQUIRE_THAT(*content, Catch::Matchers::RangeEquals(std::as_bytes(std::span{CONTENT})));

        REQUIRE(zero::filesystem::remove(path));
    }
}

TEST_CASE("read string from file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-read-string";

    SECTION("file does not exist") {
        const auto content = zero::filesystem::readString(path);
        REQUIRE_FALSE(content);
        REQUIRE(content.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        REQUIRE(zero::filesystem::write(path, std::as_bytes(std::span{CONTENT})));

        const auto content = zero::filesystem::readString(path);
        REQUIRE(content);
        REQUIRE(*content == CONTENT);

        REQUIRE(zero::filesystem::remove(path));
    }
}

TEST_CASE("write bytes to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-write-bytes";

    REQUIRE(zero::filesystem::write(path, std::as_bytes(std::span{CONTENT})));
    DEFER(REQUIRE(zero::filesystem::remove(path)));

    const auto content = zero::filesystem::read(path);
    REQUIRE(content);
    REQUIRE_THAT(*content, Catch::Matchers::RangeEquals(std::as_bytes(std::span{CONTENT})));
}

TEST_CASE("write string to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-write-string";

    REQUIRE(zero::filesystem::write(path, CONTENT));
    DEFER(REQUIRE(zero::filesystem::remove(path)));

    const auto content = zero::filesystem::readString(path);
    REQUIRE(content);
    REQUIRE(*content == CONTENT);
}

TEST_CASE("copy file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto from = *temp / "zero-filesystem-copy-file-from";
    const auto to = *temp / "zero-filesystem-copy-file-to";

    SECTION("source file does not exist") {
        const auto result = zero::filesystem::copyFile(from, to);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("destination file does not exist") {
        REQUIRE(zero::filesystem::write(from, CONTENT));
        DEFER(REQUIRE(zero::filesystem::remove(from)));

        REQUIRE(zero::filesystem::copyFile(from, to));
        DEFER(REQUIRE(zero::filesystem::remove(to)));

        const auto content = zero::filesystem::readString(to);
        REQUIRE(content);
        REQUIRE(*content == CONTENT);
    }

    SECTION("destination file exists") {
        REQUIRE(zero::filesystem::write(from, CONTENT));
        DEFER(REQUIRE(zero::filesystem::remove(from)));

        REQUIRE(zero::filesystem::write(to, ""));
        DEFER(REQUIRE(zero::filesystem::remove(to)));

        SECTION("default") {
            const auto result = zero::filesystem::copyFile(from, to);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::file_exists);
        }

        SECTION("overwrite") {
            REQUIRE(zero::filesystem::copyFile(from, to, std::filesystem::copy_options::overwrite_existing));

            const auto content = zero::filesystem::readString(to);
            REQUIRE(content);
            REQUIRE(*content == CONTENT);
        }
    }
}

TEST_CASE("create directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / "zero-filesystem-create-directory";

    SECTION("directory does not exist") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        DEFER(REQUIRE(zero::filesystem::remove(directory)));
        REQUIRE(zero::filesystem::exists(directory).value_or(false));
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        DEFER(REQUIRE(zero::filesystem::remove(directory)));

        const auto result = zero::filesystem::createDirectory(directory);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::file_exists);
    }
}

TEST_CASE("create directories", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / "zero-filesystem-create-directories";

    SECTION("directory does not exist") {
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        DEFER(REQUIRE(zero::filesystem::removeAll(directory)));
        REQUIRE(zero::filesystem::exists(directory).value_or(false));
        REQUIRE(zero::filesystem::exists(directory / "sub").value_or(false));
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        REQUIRE(zero::filesystem::exists(directory).value_or(false));
        REQUIRE(zero::filesystem::exists(directory / "sub").value_or(false));
    }

    SECTION("parent directory exists") {
        REQUIRE(zero::filesystem::createDirectories(directory));
        DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

        REQUIRE(zero::filesystem::createDirectories(directory / "sub"));
        REQUIRE(zero::filesystem::exists(directory).value_or(false));
        REQUIRE(zero::filesystem::exists(directory / "sub").value_or(false));
    }
}

TEST_CASE("remove", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-remove";

    SECTION("file") {
        SECTION("does not exist") {
            const auto result = zero::filesystem::remove(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::write(path, ""));
            REQUIRE(zero::filesystem::remove(path));
            REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));
        }
    }

    SECTION("directory") {
        SECTION("does not exist") {
            const auto result = zero::filesystem::remove(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::createDirectory(path));
            REQUIRE(zero::filesystem::remove(path));
            REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));
        }

        SECTION("not empty") {
            REQUIRE(zero::filesystem::createDirectories(path / "sub"));
            DEFER(REQUIRE(zero::filesystem::removeAll(path)));

            const auto result = zero::filesystem::remove(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::directory_not_empty);
        }
    }
}

TEST_CASE("remove all", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-remove-all";

    SECTION("file") {
        SECTION("does not exist") {
            const auto result = zero::filesystem::removeAll(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::write(path, ""));

            const auto result = zero::filesystem::removeAll(path);
            REQUIRE(result);
            REQUIRE(*result == 1);
            REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));
        }
    }

    SECTION("directory") {
        SECTION("does not exist") {
            const auto result = zero::filesystem::removeAll(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }

        SECTION("exists") {
            REQUIRE(zero::filesystem::createDirectories(path / "sub"));

            const auto result = zero::filesystem::removeAll(path);
            REQUIRE(result);
            REQUIRE(*result == 2);
            REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));
        }
    }
}

TEST_CASE("read directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / "zero-filesystem-read-directory";

    SECTION("directory does not exist") {
        const auto it = zero::filesystem::readDirectory(directory / "z");
        REQUIRE_FALSE(it);
        REQUIRE(it.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

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

    const auto directory = *temp / "zero-filesystem-walk-directory";

    SECTION("directory does not exist") {
        const auto it = zero::filesystem::walkDirectory(directory / "z");
        REQUIRE_FALSE(it);
        REQUIRE(it.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("directory exists") {
        REQUIRE(zero::filesystem::createDirectory(directory));
        DEFER(REQUIRE(zero::filesystem::removeAll(directory)));

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
                    return zero::filesystem::isRegularFile(path).value_or(false);
                })
                | std::ranges::to<std::list>(),
                Catch::Matchers::UnorderedRangeEquals(files)
            );
        }
    }
}
