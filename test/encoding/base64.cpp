#include <zero/encoding/base64.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("base64 encoding", "[base64]") {
    std::byte buffer[5] = {
            std::byte{'h'},
            std::byte{'e'},
            std::byte{'l'},
            std::byte{'l'},
            std::byte{'o'}
    };

    REQUIRE(zero::encoding::base64::encode({buffer, 0}).empty());
    REQUIRE(zero::encoding::base64::encode(buffer) == "aGVsbG8=");
    REQUIRE(!zero::encoding::base64::decode("aGVsbG8"));
    REQUIRE(zero::encoding::base64::decode("aGVsbG8=") == std::vector<std::byte>(buffer, buffer + 5));
}