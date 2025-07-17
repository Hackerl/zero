#include <catch_extensions.h>
#include <zero/encoding/hex.h>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("hex encoding", "[encoding::hex]") {
    using namespace std::string_view_literals;
    REQUIRE_THAT(zero::encoding::hex::encode(std::span<const std::byte>{}), Catch::Matchers::IsEmpty());
    REQUIRE(zero::encoding::hex::encode(std::as_bytes(std::span{"hello"sv})) == "68656c6c6f");
}

TEST_CASE("hex decoding", "[encoding::hex]") {
    SECTION("empty") {
        const auto result = zero::encoding::hex::decode("");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::IsEmpty());
    }

    SECTION("invalid length") {
        REQUIRE_ERROR(zero::encoding::hex::decode("68656c6c6"), zero::encoding::hex::DecodeError::INVALID_LENGTH);
    }

    SECTION("invalid hex character") {
        REQUIRE_ERROR(zero::encoding::hex::decode("68656c6cy6"), zero::encoding::hex::DecodeError::INVALID_HEX_CHARACTER);
    }

    SECTION("valid") {
        using namespace std::string_view_literals;
        const auto result = zero::encoding::hex::decode("68656c6c6f");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(std::as_bytes(std::span{"hello"sv})));
    }
}
