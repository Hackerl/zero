#include <zero/encoding/hex.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("hex encoding", "[encoding]") {
    constexpr std::array data{
        std::byte{'h'},
        std::byte{'e'},
        std::byte{'l'},
        std::byte{'l'},
        std::byte{'o'}
    };

    REQUIRE_THAT(zero::encoding::hex::encode({data.data(), 0}), Catch::Matchers::IsEmpty());
    REQUIRE(zero::encoding::hex::encode(data) == "68656c6c6f");

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
    REQUIRE_THAT(*result, Catch::Matchers::SizeIs(data.size()));
    REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(data));
}
