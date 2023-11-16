#include <zero/encoding/base64.h>
#include <catch2/catch_test_macros.hpp>
#include <array>

TEST_CASE("base64 encoding", "[base64]") {
    constexpr std::array buffer = {
            std::byte{'h'},
            std::byte{'e'},
            std::byte{'l'},
            std::byte{'l'},
            std::byte{'o'}
    };

    REQUIRE(zero::encoding::base64::encode({buffer.data(), 0}).empty());
    REQUIRE(zero::encoding::base64::encode(buffer) == "aGVsbG8=");

    auto result = zero::encoding::base64::decode("");
    REQUIRE(result);
    REQUIRE(result->empty());

    result = zero::encoding::base64::decode("aGVsbG8");
    REQUIRE(!result);
    REQUIRE(result.error() == std::errc::invalid_argument);

    result = zero::encoding::base64::decode("aGVsbG8=");
    REQUIRE(result);
    REQUIRE(result->size() == buffer.size());
    REQUIRE(std::equal(result->begin(), result->end(), buffer.begin()));
}