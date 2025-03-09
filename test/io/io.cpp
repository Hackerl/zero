#include <catch_extensions.h>
#include <zero/io/io.h>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("copy", "[io]") {
    const auto input = GENERATE(take(10, randomString(1, 10240)));

    zero::io::StringReader reader{input};
    zero::io::StringWriter writer;
    REQUIRE(zero::io::copy(reader, writer) == input.size());
    REQUIRE(*writer == input);
}

TEST_CASE("read all", "[io]") {
    const auto input = GENERATE(take(10, randomString(1, 10240)));

    zero::io::StringReader reader{input};

    const auto result = reader.readAll();
    REQUIRE(result);
    REQUIRE_THAT(*result, Catch::Matchers::RangeEquals(std::as_bytes(std::span{input})));
}

TEST_CASE("read exactly", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 10240)));

    SECTION("normal") {
        zero::io::BytesReader reader{input};

        std::vector<std::byte> data;
        data.resize(input.size());

        REQUIRE(reader.readExactly(data));
        REQUIRE_THAT(data, Catch::Matchers::RangeEquals(input));
    }

    SECTION("unexpected eof") {
        zero::io::BytesReader reader{{}};

        std::vector<std::byte> data;
        data.resize(input.size());

        REQUIRE_ERROR(reader.readExactly(data), zero::io::IOError::UNEXPECTED_EOF);
    }
}

TEST_CASE("string reader", "[io]") {
    const auto input = GENERATE(take(10, randomString(1, 10240)));

    zero::io::StringReader reader{input};

    std::string message;
    message.resize(input.size());

    REQUIRE(reader.read(std::as_writable_bytes(std::span{message})) == input.size());
    REQUIRE(message == input);

    REQUIRE(reader.read(std::as_writable_bytes(std::span{message})) == 0);
}

TEST_CASE("string writer", "[io]") {
    const auto input = GENERATE(take(10, randomString(1, 10240)));

    zero::io::StringWriter writer;
    REQUIRE(writer.writeAll(std::as_bytes(std::span{input})));
    REQUIRE(writer.data() == input);
    REQUIRE(*writer == input);
}

TEST_CASE("bytes reader", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 10240)));

    zero::io::BytesReader reader{input};

    std::vector<std::byte> data;
    data.resize(input.size());

    REQUIRE(reader.read(data) == input.size());
    REQUIRE_THAT(data, Catch::Matchers::RangeEquals(input));

    REQUIRE(reader.read(data) == 0);
}

TEST_CASE("bytes writer", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 10240)));

    zero::io::BytesWriter writer;
    REQUIRE(writer.writeAll(std::as_bytes(std::span{input})));
    REQUIRE_THAT(writer.data(), Catch::Matchers::RangeEquals(input));
    REQUIRE_THAT(*writer, Catch::Matchers::RangeEquals(input));
}
