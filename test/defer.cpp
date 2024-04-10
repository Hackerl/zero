#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("defer expression", "[defer]") {
    int i = 0;

    DEFER(REQUIRE(i == 15));
    DEFER(
        REQUIRE(i == 10);
        i += 5;
    );

    DEFER(
        REQUIRE(i == 6);
        i += 4;
    );

    DEFER(
        REQUIRE(i == 3);
        i += 3;
    );

    DEFER(
        REQUIRE(i == 1);
        i += 2;
    );

    DEFER(
        REQUIRE(i == 0);
        i += 1;
    );
}
