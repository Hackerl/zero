#include <zero/encoding/hex.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

constexpr std::string_view DATA = "hello";

TEST_CASE("hex encoding", "[encoding]") {
    REQUIRE_THAT(zero::encoding::hex::encode(std::span<const std::byte>{}), Catch::Matchers::IsEmpty());
    REQUIRE(zero::encoding::hex::encode(std::as_bytes(std::span{DATA})) == "68656c6c6f");
}

TEST_CASE("hex decoding", "[encoding]") {
    SECTION("empty") {
        const auto result = zero::encoding::hex::decode("");
        REQUIRE(result);
        REQUIRE(result->empty());
    }

    SECTION("invalid length") {
        const auto result = zero::encoding::hex::decode("68656c6c6");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::encoding::hex::DecodeError::INVALID_LENGTH);
    }

    SECTION("invalid hex character") {
        const auto result = zero::encoding::hex::decode("68656c6cy6");
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::encoding::hex::DecodeError::INVALID_HEX_CHARACTER);
    }

    SECTION("valid") {
        const auto result = zero::encoding::hex::decode("68656c6c6f");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::SizeIs(DATA.size()));
        REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(std::as_bytes(std::span{DATA})));
    }
}
