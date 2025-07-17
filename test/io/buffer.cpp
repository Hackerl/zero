#include "catch_extensions.h"
#include <zero/io/buffer.h>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("buffer reader", "[io::buffer]") {
    const auto input = GENERATE(take(1, randomBytes(1, 102400)));
    const auto capacity = GENERATE(1uz, take(1, random(2uz, 102400uz)));

    SECTION("capacity") {
        zero::io::BufReader reader{zero::io::BytesReader{input}, capacity};
        REQUIRE(reader.capacity() == capacity);
    }

    SECTION("available") {
        zero::io::BufReader reader{zero::io::BytesReader{input}, capacity};

        SECTION("empty") {
            REQUIRE(reader.available() == 0);
        }

        SECTION("not empty") {
            std::vector<std::byte> data;
            REQUIRE(reader.read(data) == 0);
            REQUIRE(reader.available() == (std::min)(input.size(), capacity));
        }
    }

    SECTION("read") {
        zero::io::BufReader reader{zero::io::BytesReader{input}, capacity};

        SECTION("normal") {
            const auto size = GENERATE_REF(take(1, random(1uz, input.size() * 2)));

            std::vector<std::byte> data;
            data.resize(size);

            const auto n = reader.read(data);
            REQUIRE(n == (std::min)(size, input.size()));

            data.resize(*n);
            REQUIRE_THAT(data, Catch::Matchers::RangeEquals(std::span{input.data(), *n}));
        }

        SECTION("eof") {
            REQUIRE(reader.readAll());
            std::array<std::byte, 64> data{};
            REQUIRE(reader.read(data) == 0);
        }
    }

    SECTION("peek") {
        zero::io::BufReader reader{zero::io::BytesReader{input}, capacity};

        SECTION("normal") {
            const auto limit = (std::min)(input.size(), capacity);
            const auto size = GENERATE_REF(take(1, random(1uz, limit)));

            std::vector<std::byte> data;
            data.resize(size);

            REQUIRE(reader.peek(data));
            REQUIRE_THAT(data, Catch::Matchers::RangeEquals(std::span{input.data(), size}));
            REQUIRE(reader.available() == limit);
        }

        SECTION("invalid argument") {
            const auto size = GENERATE_REF(take(1, random(capacity + 1, capacity * 2)));

            std::vector<std::byte> data;
            data.resize(size);
            REQUIRE_ERROR(reader.peek(data), std::errc::invalid_argument);
        }
    }

    auto inputString = GENERATE(take(1, randomAlphanumericString(1, 102400)));

    SECTION("read line") {
        SECTION("normal") {
            const auto pos = GENERATE_REF(take(1, random(0uz, inputString.size() - 1)));

            SECTION("CRLF") {
                inputString.insert(inputString.begin() + static_cast<std::ptrdiff_t>(pos), '\r');
                inputString.insert(inputString.begin() + static_cast<std::ptrdiff_t>(pos) + 1, '\n');
            }

            SECTION("LF") {
                inputString.insert(inputString.begin() + static_cast<std::ptrdiff_t>(pos), '\n');
            }

            zero::io::BufReader reader{zero::io::StringReader{inputString}, capacity};
            REQUIRE(reader.readLine() ==inputString.substr(0, pos));
        }

        SECTION("unexpected eof") {
            zero::io::BufReader reader{zero::io::StringReader{inputString}, capacity};
            REQUIRE_ERROR(reader.readLine(), zero::io::Error::UNEXPECTED_EOF);
        }
    }

    SECTION("read until") {
        const auto c = GENERATE('\t', '\n', '\r', '\x0b', '\x0c');

        SECTION("normal") {
            const auto pos = GENERATE_REF(take(1, random(0uz, inputString.size() - 1)));

            inputString.insert(inputString.begin() + static_cast<std::ptrdiff_t>(pos), c);
            zero::io::BufReader reader{zero::io::StringReader{inputString}, capacity};

            const auto data = reader.readUntil(static_cast<std::byte>(c));
            REQUIRE(data);
            REQUIRE_THAT(*data, Catch::Matchers::RangeEquals(std::as_bytes(std::span{inputString.data(), pos})));
        }

        SECTION("unexpected eof") {
            zero::io::BufReader reader{zero::io::StringReader{inputString}, capacity};
            REQUIRE_ERROR(reader.readUntil(static_cast<std::byte>(c)), zero::io::Error::UNEXPECTED_EOF);
        }
    }
}

TEST_CASE("buffer writer", "[io::buffer]") {
    const auto input = GENERATE(take(1, randomBytes(1, 102400)));
    const auto capacity = GENERATE(1uz, take(1, random(2uz, 102400uz)));

    const auto bytesWriter = std::make_shared<zero::io::BytesWriter>();
    zero::io::BufWriter writer{bytesWriter, capacity};

    SECTION("capacity") {
        REQUIRE(writer.capacity() == capacity);
    }

    SECTION("pending") {
        SECTION("empty") {
            REQUIRE(writer.pending() == 0);
        }

        SECTION("not empty") {
            REQUIRE(writer.writeAll(input));
            REQUIRE(writer.pending() > 0);
        }
    }

    SECTION("write") {
        REQUIRE(writer.write(input) == input.size());
        REQUIRE(writer.pending() > 0);
    }

    SECTION("flush") {
        REQUIRE(writer.writeAll(input));
        REQUIRE(writer.flush());
        REQUIRE(writer.pending() == 0);
        REQUIRE_THAT(bytesWriter->data(), Catch::Matchers::RangeEquals(input));
    }
}

static_assert(std::is_constructible_v<zero::io::BufReader<zero::io::StringReader>, zero::io::StringReader>);

static_assert(
    std::is_constructible_v<
        zero::io::BufReader<std::unique_ptr<zero::io::IReader>>,
        std::unique_ptr<zero::io::IReader>
    >
);

static_assert(
    std::is_constructible_v<
        zero::io::BufReader<std::shared_ptr<zero::io::IReader>>,
        std::shared_ptr<zero::io::IReader>
    >
);

static_assert(std::is_constructible_v<zero::io::BufWriter<zero::io::StringWriter>, zero::io::StringWriter>);

static_assert(
    std::is_constructible_v<
        zero::io::BufWriter<std::unique_ptr<zero::io::IWriter>>,
        std::unique_ptr<zero::io::IWriter>
    >
);

static_assert(
    std::is_constructible_v<
        zero::io::BufWriter<std::shared_ptr<zero::io::IWriter>>,
        std::shared_ptr<zero::io::IWriter>
    >
);
