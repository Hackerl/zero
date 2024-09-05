#include <zero/atomic/event.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

TEST_CASE("notify event", "[atomic]") {
    SECTION("auto") {
        using namespace std::chrono_literals;

        zero::atomic::Event event;

        auto result = event.wait(10ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::timed_out);

        int n = 0;

        std::thread thread(
            [&] {
                std::this_thread::sleep_for(10ms);
                n = 1;
                event.set();
            }
        );

        REQUIRE(event.wait());
        REQUIRE(n == 1);

        thread.join();

        result = event.wait(10ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::timed_out);
    }

    SECTION("manual") {
        SECTION("not set initially") {
            using namespace std::chrono_literals;

            zero::atomic::Event event(true);

            auto result = event.wait(10ms);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::timed_out);

            int n = 0;

            std::thread thread(
                [&] {
                    std::this_thread::sleep_for(10ms);
                    n = 1;
                    event.set();
                }
            );

            REQUIRE(event.wait());
            REQUIRE(n == 1);

            thread.join();

            REQUIRE(event.wait());
            event.reset();

            result = event.wait(10ms);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::timed_out);
        }

        SECTION("initial set") {
            using namespace std::chrono_literals;

            zero::atomic::Event event(true, true);
            REQUIRE(event.wait());
            event.reset();

            auto result = event.wait(10ms);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::timed_out);

            int n = 0;

            std::thread thread(
                [&] {
                    std::this_thread::sleep_for(10ms);
                    n = 1;
                    event.set();
                }
            );

            REQUIRE(event.wait());
            REQUIRE(n == 1);

            thread.join();

            REQUIRE(event.wait());
            event.reset();

            result = event.wait(10ms);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::timed_out);
        }
    }
}
