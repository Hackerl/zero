#include <zero/atomic/circular_buffer.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("lock-free circular buffer", "[circular buffer]") {
    zero::atomic::CircularBuffer<int> buffer(10);

    REQUIRE(buffer.empty());
    REQUIRE(buffer.empty());
    REQUIRE(!buffer.full());

    SECTION("producer/consumer") {
        auto index = buffer.reserve();

        REQUIRE(index);
        REQUIRE(!buffer.empty());
        REQUIRE(buffer.size() == 1);
        REQUIRE(!buffer.full());

        buffer[*index] = 1;
        buffer.commit(*index);

        index = buffer.acquire();

        REQUIRE(index);
        REQUIRE(buffer.empty());
        REQUIRE(!buffer.full());

        REQUIRE(buffer[*index] == 1);

        buffer.release(*index);
    }

    SECTION("full buffer") {
        for (std::size_t i = 0; i < 9; i++) {
            const auto index = buffer.reserve();
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