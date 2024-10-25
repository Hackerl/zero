#include <zero/encoding/base64.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

constexpr std::string_view DATA = "hello";

TEST_CASE("base64 encoding", "[encoding]") {
    REQUIRE_THAT(zero::encoding::base64::encode(std::span<const std::byte>{}), Catch::Matchers::IsEmpty());
    REQUIRE(zero::encoding::base64::encode(std::as_bytes(std::span{DATA})) == "aGVsbG8=");
}

TEST_CASE("base64 decoding", "[encoding]") {
    SECTION("empty") {
        const auto result = zero::encoding::base64::decode("");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::IsEmpty());
    }

    SECTION("invalid length") {
        const auto result = zero::encoding::base64::decode("aGVsbG8");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::encoding::base64::DecodeError::INVALID_LENGTH);
    }

    SECTION("valid") {
        const auto result = zero::encoding::base64::decode("aGVsbG8=");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::SizeIs(DATA.size()));
        REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(std::as_bytes(std::span{DATA})));
    }
}
