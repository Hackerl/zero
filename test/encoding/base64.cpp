#include <zero/encoding/base64.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("base64 encoding", "[encoding]") {
    constexpr std::array data{
        std::byte{'h'},
        std::byte{'e'},
        std::byte{'l'},
        std::byte{'l'},
        std::byte{'o'}
    };

    REQUIRE_THAT(zero::encoding::base64::encode({data.data(), 0}), Catch::Matchers::IsEmpty());
    REQUIRE(zero::encoding::base64::encode(data) == "aGVsbG8=");

    auto result = zero::encoding::base64::decode("");
    REQUIRE(result);
    REQUIRE_THAT(*result, Catch::Matchers::IsEmpty());

    result = zero::encoding::base64::decode("aGVsbG8");
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == zero::encoding::base64::DecodeError::INVALID_LENGTH);

    result = zero::encoding::base64::decode("aGVsbG8=");
    REQUIRE(result);
    REQUIRE_THAT(*result, Catch::Matchers::SizeIs(data.size()));
    REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(data));
}
