#include <zero/atomic/event.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("notify event", "[event]") {
    zero::atomic::Event event;

    int n = 0;

    SECTION("normal notification") {
        std::thread thread(
                [&] {
                    std::this_thread::sleep_for(100ms);
                    n = 1;
                    event.notify();
                }
        );

        REQUIRE(event.wait());
        REQUIRE(n == 1);

        thread.join();
    }

    SECTION("wait timeout") {
        std::thread thread(
                [&] {
                    std::this_thread::sleep_for(500ms);
                    n = 1;
                    event.notify();
                }
        );

        REQUIRE(event.wait(100ms).error() == std::errc::timed_out);
        REQUIRE(n == 0);

        thread.join();
    }
}