#include "catch_extensions.h"
#include <zero/defer.h>

TEST_CASE("defer expression", "[defer]") {
    int i{0};

    Z_DEFER(REQUIRE(i == 15));
    Z_DEFER(
        REQUIRE(i == 10);
        i += 5;
    );

    Z_DEFER(
        REQUIRE(i == 6);
        i += 4;
    );

    Z_DEFER(
        REQUIRE(i == 3);
        i += 3;
    );

    Z_DEFER(
        REQUIRE(i == 1);
        i += 2;
    );

    Z_DEFER(
        REQUIRE(i == 0);
        i += 1;
    );
}
