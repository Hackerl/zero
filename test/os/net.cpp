#include <zero/os/net.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("stringify IPv4", "[net]") {
    REQUIRE(zero::os::net::stringifyIP(
            std::array<std::byte, 4>{}
    ) == "0.0.0.0");

    REQUIRE(zero::os::net::stringifyIP(
            std::array<std::byte, 4>{std::byte{127}, std::byte{0}, std::byte{0}, std::byte{1}}
    ) == "127.0.0.1");

    REQUIRE(zero::os::net::stringifyIP(
            std::array<std::byte, 4>{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}
    ) == "255.255.255.255");
}

TEST_CASE("stringify IPv6", "[net]") {
    REQUIRE(zero::os::net::stringifyIP(
            std::array<std::byte, 16>{}
    ) == "::");

    REQUIRE(zero::os::net::stringifyIP(
            std::array<std::byte, 16>{
                    std::byte{253},
                    std::byte{189},
                    std::byte{220},
                    std::byte{2},
                    std::byte{0},
                    std::byte{255},
                    std::byte{0},
                    std::byte{1},
                    std::byte{0},
                    std::byte{9},
                    std::byte{0},
                    std::byte{0},
                    std::byte{0},
                    std::byte{0},
                    std::byte{0},
                    std::byte{141}
            }
    ) == "fdbd:dc02:ff:1:9::8d");
}

TEST_CASE("fetch network interface", "[net]") {
    REQUIRE(zero::os::net::interfaces());
}