#include <catch_extensions.h>
#include <zero/atomic/circular_buffer.h>

TEST_CASE("lock-free circular buffer", "[atomic::circular_buffer]") {
    zero::atomic::CircularBuffer<int> buffer{10};

    REQUIRE(buffer.empty());
    REQUIRE(buffer.empty());
    REQUIRE_FALSE(buffer.full());

    SECTION("producer/consumer") {
        auto index = buffer.reserve();
        REQUIRE(index);
        REQUIRE_FALSE(buffer.empty());
        REQUIRE(buffer.size() == 1);
        REQUIRE_FALSE(buffer.full());

        buffer[*index] = 1;
        buffer.commit(*index);

        index = buffer.acquire();
        REQUIRE(index);
        REQUIRE(buffer.empty());
        REQUIRE_FALSE(buffer.full());

        REQUIRE(buffer[*index] == 1);
        buffer.release(*index);
    }

    SECTION("full buffer") {
        for (std::size_t i{0}; i < 9; i++) {
            const auto index = buffer.reserve();
            REQUIRE(index);

            buffer[*index] = 1;
            buffer.commit(*index);
        }

        REQUIRE_FALSE(buffer.reserve());
        REQUIRE_FALSE(buffer.empty());
        REQUIRE(buffer.size() == 9);
        REQUIRE(buffer.full());
    }
}
