#include <zero/atomic/circular_buffer.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("lock-free circular buffer", "[circular buffer]") {
    zero::atomic::CircularBuffer<int, 10> buffer;

    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
    REQUIRE(!buffer.full());

    SECTION("producer/consumer") {
        std::optional<size_t> index = buffer.reserve();

        REQUIRE(index);
        REQUIRE(!buffer.empty());
        REQUIRE(buffer.size() == 1);
        REQUIRE(!buffer.full());

        buffer[*index] = 1;

        buffer.commit(*index);

        index = buffer.acquire();

        REQUIRE(index);
        REQUIRE(buffer.empty());
        REQUIRE(buffer.size() == 0);
        REQUIRE(!buffer.full());

        REQUIRE(buffer[*index] == 1);

        buffer.release(*index);
    }

    SECTION("full buffer") {
        for (size_t i = 0; i < 9; i++) {
            std::optional<size_t> index = buffer.reserve();
            REQUIRE(index);

            buffer[*index] = 1;
            buffer.commit(*index);
        }

        REQUIRE(!buffer.reserve());
        REQUIRE(!buffer.empty());
        REQUIRE(buffer.size() == 9);
        REQUIRE(buffer.full());
    }
}