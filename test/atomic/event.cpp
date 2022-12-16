#include <thread>
#include <zero/atomic/event.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("notify event", "[event]") {
    zero::atomic::Event event;

    int n = 0;

    SECTION("normal notification") {
        std::thread thread(
                [](zero::atomic::Event *event, int *n) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});

                    *n = 1;
                    event->notify();
                },
                &event,
                &n
        );

        event.wait();
        REQUIRE(n == 1);

        thread.join();
    }

    SECTION("wait timeout") {
        std::thread thread(
                [](zero::atomic::Event *event, int *n) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{500});

                    *n = 1;
                    event->notify();
                },
                &event,
                &n
        );

        event.wait(std::chrono::milliseconds{100});
        REQUIRE(n == 0);

        thread.join();
    }
}