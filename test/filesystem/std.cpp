#include <zero/filesystem/std.h>
#include <zero/filesystem/fs.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <list>

TEST_CASE("std::filesystem wrapper", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    SECTION("directory traversal") {
        const auto directory = *temp / "zero-filesystem";

        SECTION("read") {
            auto it = zero::filesystem::readDirectory(directory / "z");
            REQUIRE(!it);
            REQUIRE(it.error() == std::errc::no_such_file_or_directory);
            REQUIRE(zero::filesystem::createDirectory(directory));

            const std::list files = {directory / "a", directory / "b", directory / "c"};

            for (const auto &file: files) {
                REQUIRE(zero::filesystem::write(file, ""));
            }

            it = zero::filesystem::readDirectory(directory);
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

            REQUIRE(zero::filesystem::removeAll(directory));
        }

        SECTION("walk") {
            auto it = zero::filesystem::walkDirectory(directory / "z");
            REQUIRE(!it);
            REQUIRE(it.error() == std::errc::no_such_file_or_directory);
            REQUIRE(zero::filesystem::createDirectory(directory));

            const std::list files = {directory / "a", directory / "b" / "c", directory / "d" / "e" / "f"};

            for (const auto &file: files) {
                REQUIRE(zero::filesystem::createDirectories(file.parent_path()));
                REQUIRE(zero::filesystem::write(file, ""));
            }

            it = zero::filesystem::walkDirectory(directory);
            REQUIRE(it);

#ifndef _WIN32
            SECTION("error while traversing") {
                REQUIRE(zero::filesystem::permissions(directory / "b", std::filesystem::perms::none));

                auto entries = *it | std::ranges::to<std::list>();
                REQUIRE(!entries.back());
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

            REQUIRE(zero::filesystem::removeAll(directory));
        }
    }
}
