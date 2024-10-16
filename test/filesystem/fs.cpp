#include <zero/filesystem/fs.h>
#include <zero/filesystem/std.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("filesystem api", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem";

    SECTION("no such file") {
        SECTION("bytes") {
            const auto result = zero::filesystem::read(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }

        SECTION("string") {
            const auto result = zero::filesystem::readString(path);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }
    }

    SECTION("read and write") {
        constexpr std::string_view data{"hello"};

        SECTION("bytes") {
            REQUIRE(zero::filesystem::write(path, as_bytes(nonstd::span{data})));

            const auto content = zero::filesystem::read(path);
            REQUIRE(content);
            REQUIRE_THAT(*content, Catch::Matchers::RangeEquals(as_bytes(nonstd::span{data})));
        }

        SECTION("string") {
            REQUIRE(zero::filesystem::write(path, data));

            const auto content = zero::filesystem::readString(path);
            REQUIRE(content);
            REQUIRE(*content == data);
        }
    }

    REQUIRE(zero::filesystem::remove(path));
}
