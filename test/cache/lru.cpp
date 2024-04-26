#include <zero/cache/lru.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("LRU cache", "[cache]") {
    zero::cache::LRUCache<int, std::string> cache(5);

    REQUIRE(cache.capacity() == 5);
    REQUIRE(cache.empty());

    SECTION("lookup/insert cache") {
        REQUIRE(!cache.get(0));
        cache.set(0, "hello");

        REQUIRE(cache.size() == 1);

        auto value = cache.get(0);
        REQUIRE(value);
        REQUIRE(value->get() == "hello");

        cache.set(0, "world");
        cache.set(1, "hello");

        REQUIRE(cache.size() == 2);

        value = cache.get(0);
        REQUIRE(value);
        REQUIRE(value->get() == "world");

        value = cache.get(1);
        REQUIRE(value);
        REQUIRE(value->get() == "hello");
    }

    SECTION("evict cache") {
        cache.set(0, "0");
        cache.set(1, "1");
        cache.set(2, "2");
        cache.set(3, "3");
        cache.set(4, "4");

        REQUIRE(cache.size() == 5);
        cache.set(5, "5");

        REQUIRE(cache.size() == 5);
        REQUIRE(!cache.get(0));

        cache.set(1, "1!");
        cache.set(6, "6");

        REQUIRE(cache.size() == 5);

        const auto value = cache.get(1);
        REQUIRE(value);
        REQUIRE(value->get() == "1!");
        REQUIRE(!cache.get(2));
    }
}
