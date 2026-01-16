#include <catch_extensions.h>
#include <zero/io/io.h>

TEST_CASE("copy", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 102400)));

    zero::io::BytesReader reader{input};
    zero::io::BytesWriter writer;
    REQUIRE(zero::io::copy(reader, writer) == input.size());
    REQUIRE(*writer == input);
}

TEST_CASE("read all", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 102400)));

    zero::io::BytesReader reader{input};
    REQUIRE(reader.readAll() == input);
}

TEST_CASE("read exactly", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 102400)));

    SECTION("normal") {
        zero::io::BytesReader reader{input};

        std::vector<std::byte> data;
        data.resize(input.size());

        REQUIRE(reader.readExactly(data));
        REQUIRE(data == input);
    }

    SECTION("unexpected eof") {
        zero::io::BytesReader reader{{}};

        std::vector<std::byte> data;
        data.resize(input.size());

        REQUIRE_ERROR(reader.readExactly(data), zero::io::Error::UnexpectedEOF);
    }
}

TEST_CASE("string reader", "[io]") {
    const auto input = GENERATE(take(10, randomString(1, 102400)));

    zero::io::StringReader reader{input};

    std::string message;
    message.resize(input.size());

    REQUIRE(reader.read(std::as_writable_bytes(std::span{message})) == input.size());
    REQUIRE(message == input);

    REQUIRE(reader.read(std::as_writable_bytes(std::span{message})) == 0);
}

TEST_CASE("string writer", "[io]") {
    const auto input = GENERATE(take(10, randomString(1, 102400)));

    zero::io::StringWriter writer;
    REQUIRE(writer.writeAll(std::as_bytes(std::span{input})));
    REQUIRE(writer.data() == input);
    REQUIRE(*writer == input);
}

TEST_CASE("bytes reader", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 102400)));

    zero::io::BytesReader reader{input};

    std::vector<std::byte> data;
    data.resize(input.size());

    REQUIRE(reader.read(data) == input.size());
    REQUIRE(data == input);

    REQUIRE(reader.read(data) == 0);
}

TEST_CASE("bytes writer", "[io]") {
    const auto input = GENERATE(take(10, randomBytes(1, 102400)));

    zero::io::BytesWriter writer;
    REQUIRE(writer.writeAll(input));
    REQUIRE(writer.data() == input);
    REQUIRE(*writer == input);
}
