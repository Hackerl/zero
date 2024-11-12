#include <zero/atomic/event.h>
#include <zero/defer.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

TEST_CASE("auto-reset event", "[atomic]") {
    using namespace std::chrono_literals;

    zero::atomic::Event event;

    SECTION("normal") {
        std::atomic<int> n{};

        std::thread thread{
            [&] {
                std::this_thread::sleep_for(10ms);
                n = 1;
                event.set();
            }
        };

        REQUIRE(event.wait());
        REQUIRE(n == 1);
        DEFER(thread.join());

        const auto result = event.wait(10ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::timed_out);
    }

    SECTION("timeout") {
        const auto result = event.wait(10ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::timed_out);
    }
}

TEST_CASE("manual-reset event", "[atomic]") {
    SECTION("not set initially") {
        using namespace std::chrono_literals;

        zero::atomic::Event event{true};

        SECTION("normal") {
            std::atomic<int> n{};

            std::thread thread{
                [&] {
                    std::this_thread::sleep_for(10ms);
                    n = 1;
                    event.set();
                }
            };
            DEFER(thread.join());

            REQUIRE(event.wait());
            REQUIRE(event.wait());
            REQUIRE(n == 1);

            event.reset();

            const auto result = event.wait(10ms);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::timed_out);
        }

        SECTION("timeout") {
            const auto result = event.wait(10ms);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == std::errc::timed_out);
        }
    }

    SECTION("initial set") {
        using namespace std::chrono_literals;

        zero::atomic::Event event{true, true};

        REQUIRE(event.wait());
        REQUIRE(event.wait());
        event.reset();

        const auto result = event.wait(10ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::timed_out);
    }
}
