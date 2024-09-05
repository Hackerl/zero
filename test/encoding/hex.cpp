#include <zero/encoding/hex.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("hex encoding", "[encoding]") {
    constexpr std::array buffer = {
        std::byte{'h'},
        std::byte{'e'},
        std::byte{'l'},
        std::byte{'l'},
        std::byte{'o'}
    };

    REQUIRE(zero::encoding::hex::encode({buffer.data(), 0}).empty());
    REQUIRE(zero::encoding::hex::encode(buffer) == "68656c6c6f");

    auto result = zero::encoding::hex::decode("");
    REQUIRE(result);
    REQUIRE(result->empty());

    result = zero::encoding::hex::decode("68656c6c6");
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == zero::encoding::hex::DecodeError::INVALID_LENGTH);

    result = zero::encoding::hex::decode("68656c6cy6");
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == zero::encoding::hex::DecodeError::INVALID_HEX_CHARACTER);

    result = zero::encoding::hex::decode("68656c6c6f");
    REQUIRE(result);
    REQUIRE(result->size() == buffer.size());
    REQUIRE(std::equal(result->begin(), result->end(), buffer.begin()));
}
