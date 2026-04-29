#include <catch_extensions.h>
#include <zero/atomic/circular_buffer.h>
#include <zero/error.h>

TEST_CASE("lock-free circular buffer", "[atomic::circular_buffer]") {
    const auto capacity = GENERATE(2uz, take(1, random(3uz, 1024uz)));
    const auto element = GENERATE(take(1, randomString(1, 1024)));

    zero::atomic::CircularBuffer<std::string> buffer{capacity};

    SECTION("size") {
        const auto size = GENERATE_REF(take(1, random(0uz, capacity - 1)));

        for (std::size_t i{0}; i < size; ++i) {
            const auto index = buffer.reserve();

            if (!index)
                throw zero::error::StacktraceError<std::runtime_error>{"Failed to reserve buffer slot"};

            buffer[*index] = element;
            buffer.commit(*index);
        }

        REQUIRE(buffer.size() == size);
    }

    SECTION("capacity") {
        REQUIRE(buffer.capacity() == capacity);
    }

    SECTION("is empty") {
        SECTION("empty") {
            REQUIRE(buffer.empty());
        }

        SECTION("not empty") {
            const auto index = buffer.reserve();

            if (!index)
                throw zero::error::StacktraceError<std::runtime_error>{"Failed to reserve buffer slot"};

            buffer[*index] = element;
            buffer.commit(*index);
            REQUIRE_FALSE(buffer.empty());
        }
    }

    SECTION("is full") {
        SECTION("not full") {
            REQUIRE_FALSE(buffer.full());
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity - 1; ++i) {
                const auto index = buffer.reserve();

                if (!index)
                    throw zero::error::StacktraceError<std::runtime_error>{"Failed to reserve buffer slot"};

                buffer[*index] = element;
                buffer.commit(*index);
            }

            REQUIRE(buffer.full());
        }
    }

    SECTION("subscript access") {
        {
            const auto index = buffer.reserve();

            if (!index)
                throw zero::error::StacktraceError<std::runtime_error>{"Failed to reserve buffer slot"};

            buffer[*index] = element;
            buffer.commit(*index);
        }

        const auto index = buffer.acquire();
        REQUIRE(index);
        REQUIRE(buffer[*index] == element);
        buffer.release(*index);
    }

    SECTION("reserve") {
        SECTION("success") {
            REQUIRE(buffer.reserve() == 0);
        }

        SECTION("failure") {
            for (std::size_t i{0}; i < capacity - 1; ++i) {
                if (!buffer.reserve())
                    throw zero::error::StacktraceError<std::runtime_error>{"Failed to reserve buffer slot"};
            }

            REQUIRE_FALSE(buffer.reserve());
        }
    }

    SECTION("acquire") {
        SECTION("success") {
            const auto index = buffer.reserve();

            if (!index)
                throw zero::error::StacktraceError<std::runtime_error>{"Failed to reserve buffer slot"};

            buffer[*index] = element;
            buffer.commit(*index);

            REQUIRE(buffer.acquire() == index);
        }

        SECTION("failure") {
            REQUIRE_FALSE(buffer.acquire());
        }
    }
}
