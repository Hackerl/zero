#include <catch_extensions.h>
#include <zero/encoding/base64.h>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("base64 encoding", "[encoding::base64]") {
    using namespace std::string_view_literals;
    REQUIRE_THAT(zero::encoding::base64::encode(std::span<const std::byte>{}), Catch::Matchers::IsEmpty());
    REQUIRE(zero::encoding::base64::encode(std::as_bytes(std::span{"hello"sv})) == "aGVsbG8=");
}

TEST_CASE("base64 decoding", "[encoding::base64]") {
    SECTION("empty") {
        const auto result = zero::encoding::base64::decode("");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::IsEmpty());
    }

    SECTION("invalid length") {
        REQUIRE_ERROR(zero::encoding::base64::decode("aGVsbG8"), zero::encoding::base64::DecodeError::INVALID_LENGTH);
    }

    SECTION("valid") {
        using namespace std::string_view_literals;
        const auto result = zero::encoding::base64::decode("aGVsbG8=");
        REQUIRE(result);
        REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(std::as_bytes(std::span{"hello"sv})));
    }
}
