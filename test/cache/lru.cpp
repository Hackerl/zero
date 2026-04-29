#include <catch_extensions.h>
#include <zero/cache/lru.h>

TEST_CASE("LRU cache", "[cache::lru]") {
    const auto capacity = GENERATE(1uz, take(1, random(2uz, 1024uz)));

    zero::cache::LRUCache<std::size_t, std::string> cache{capacity};

    SECTION("contains") {
        SECTION("exists") {
            cache.set(0, "0");
            REQUIRE(cache.contains(0));
        }

        SECTION("does not exist") {
            REQUIRE_FALSE(cache.contains(0));
        }
    }

    SECTION("size") {
        const auto size = GENERATE_REF(take(1, random(0uz, 1024uz)));

        for (std::size_t i{0}; i < size; ++i)
            cache.set(i, std::to_string(i));

        REQUIRE(cache.size() == (std::min)(size, capacity));
    }

    SECTION("capacity") {
        REQUIRE(cache.capacity() == capacity);
    }

    SECTION("is empty") {
        SECTION("empty") {
            REQUIRE(cache.empty());
        }

        SECTION("not empty") {
            cache.set(0, "0");
            REQUIRE_FALSE(cache.empty());
        }
    }

    SECTION("get") {
        SECTION("exists") {
            cache.set(0, "0");
            const auto value = cache.get(0);
            REQUIRE(value);
            REQUIRE(value->get() == "0");
        }

        SECTION("does not exist") {
            REQUIRE_FALSE(cache.get(0));
        }
    }

    SECTION("set") {
        SECTION("new key") {
            cache.set(0, "0");
            const auto value = cache.get(0);
            REQUIRE(value);
            REQUIRE(value->get() == "0");
        }

        SECTION("existing key") {
            cache.set(0, "0");
            cache.set(0, "1");
            const auto value = cache.get(0);
            REQUIRE(value);
            REQUIRE(value->get() == "1");
        }

        SECTION("evict") {
            for (std::size_t i{0}; i < capacity; ++i)
                cache.set(i, std::to_string(i));

            REQUIRE(cache.size() == capacity);
            cache.set(capacity, std::to_string(capacity));
            REQUIRE(cache.size() == capacity);
            REQUIRE_FALSE(cache.get(0));

            const auto value = cache.get(capacity);
            REQUIRE(value);
            REQUIRE(value->get() == std::to_string(capacity));
        }
    }
}
