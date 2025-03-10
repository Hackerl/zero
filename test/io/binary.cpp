#include "catch_extensions.h"
#include <zero/io/binary.h>
#include <catch2/catch_template_test_macros.hpp>

TEMPLATE_TEST_CASE(
    "binary transfer",
    "[io::binary]",
    std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t
) {
    const auto input = GENERATE(
        take(100, random((std::numeric_limits<TestType>::min)(), (std::numeric_limits<TestType>::max)()))
    );

    SECTION("little endian") {
        zero::io::BytesWriter writer;
        REQUIRE(zero::io::binary::writeLE(writer, input));

        zero::io::BytesReader reader{*std::move(writer)};
        REQUIRE(zero::io::binary::readLE<TestType>(reader) == input);
    }

    SECTION("big endian") {
        zero::io::BytesWriter writer;
        REQUIRE(zero::io::binary::writeBE(writer, input));

        zero::io::BytesReader reader{*std::move(writer)};
        REQUIRE(zero::io::binary::readBE<TestType>(reader) == input);
    }
}
