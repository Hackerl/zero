#include <zero/filesystem/std.h>
#include <zero/filesystem/fs.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <list>

TEST_CASE("read directory", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto directory = *temp / "zero-filesystem-read-directory";

    SECTION("directory not exists") {
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

    SECTION("directory not exists") {
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
