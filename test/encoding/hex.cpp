#include <zero/encoding/hex.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("hex encoding", "[base64]") {
    std::byte buffer[5] = {
            std::byte{'h'},
            std::byte{'e'},
            std::byte{'l'},
            std::byte{'l'},
            std::byte{'o'}
    };

    REQUIRE(zero::encoding::hex::encode(buffer, 0).empty());
    REQUIRE(zero::encoding::hex::encode(buffer, 5) == "68656c6c6f");
    REQUIRE(!zero::encoding::hex::decode("68656c6c6"));
    REQUIRE(zero::encoding::hex::decode("68656c6c6f") == std::vector<std::byte>(buffer, buffer + 5));
}