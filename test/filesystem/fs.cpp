#include <zero/filesystem/fs.h>
#include <zero/filesystem/std.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

constexpr std::string_view CONTENT = "hello";

TEST_CASE("read bytes from file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-read-bytes";
    REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));

    SECTION("file does not exist") {
        const auto content = zero::filesystem::read(path);
        REQUIRE_FALSE(content);
        REQUIRE(content.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        REQUIRE(zero::filesystem::write(path, std::as_bytes(std::span{CONTENT})));
        REQUIRE(zero::filesystem::exists(path).value_or(false));

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
    REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));

    SECTION("file does not exist") {
        const auto content = zero::filesystem::readString(path);
        REQUIRE_FALSE(content);
        REQUIRE(content.error() == std::errc::no_such_file_or_directory);
    }

    SECTION("file exists") {
        REQUIRE(zero::filesystem::write(path, std::as_bytes(std::span{CONTENT})));
        REQUIRE(zero::filesystem::exists(path).value_or(false));

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

    REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));

    REQUIRE(zero::filesystem::write(path, std::as_bytes(std::span{CONTENT})));
    REQUIRE(zero::filesystem::exists(path).value_or(false));
    DEFER(REQUIRE(zero::filesystem::remove(path)));

    const auto content = zero::filesystem::read(path);
    REQUIRE(content);
    REQUIRE_THAT(*content, Catch::Matchers::RangeEquals(std::as_bytes(std::span{CONTENT})));
}

TEST_CASE("write string to file", "[filesystem]") {
    const auto temp = zero::filesystem::temporaryDirectory();
    REQUIRE(temp);

    const auto path = *temp / "zero-filesystem-write-string";

    REQUIRE_FALSE(zero::filesystem::exists(path).value_or(false));

    REQUIRE(zero::filesystem::write(path, CONTENT));
    REQUIRE(zero::filesystem::exists(path).value_or(false));
    DEFER(REQUIRE(zero::filesystem::remove(path)));

    const auto content = zero::filesystem::readString(path);
    REQUIRE(content);
    REQUIRE(*content == CONTENT);
}
