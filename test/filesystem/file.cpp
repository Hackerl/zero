#include <zero/filesystem/file.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>

TEST_CASE("file utils", "[filesystem]") {
    const auto path = std::filesystem::temp_directory_path() / "zero-filesystem";

    SECTION("no such file") {
        SECTION("bytes") {
            const auto result = zero::filesystem::read(path);
            REQUIRE(!result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }

        SECTION("string") {
            const auto result = zero::filesystem::readString(path);
            REQUIRE(!result);
            REQUIRE(result.error() == std::errc::no_such_file_or_directory);
        }
    }

    SECTION("read and write") {
        SECTION("bytes") {
            constexpr std::array data = {
                std::byte{'h'},
                std::byte{'e'},
                std::byte{'l'},
                std::byte{'l'},
                std::byte{'o'}
            };

            const auto result = zero::filesystem::write(path, data);
            REQUIRE(result);

            const auto content = zero::filesystem::read(path);
            REQUIRE(content);
            REQUIRE(std::ranges::equal(*content, data));
        }

        SECTION("string") {
            constexpr auto data = "hello";
            const auto result = zero::filesystem::writeString(path, data);
            REQUIRE(result);

            const auto content = zero::filesystem::readString(path);
            REQUIRE(content);
            REQUIRE(*content == data);
        }
    }

    std::filesystem::remove(path);
}
