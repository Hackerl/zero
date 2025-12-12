#include <catch_extensions.h>
#include <zero/atomic/event.h>
#include <zero/defer.h>
#include <thread>

TEST_CASE("auto-reset event", "[atomic::event]") {
    using namespace std::chrono_literals;

    zero::atomic::Event event;
    REQUIRE_FALSE(event.isSet());

    SECTION("normal") {
        std::atomic<int> n;

        std::thread thread{
            [&] {
                std::this_thread::sleep_for(10ms);
                n = 1;
                event.set();
            }
        };
        Z_DEFER(thread.join());

        REQUIRE(event.wait());
        REQUIRE_FALSE(event.isSet());
        REQUIRE(n == 1);
    }

    SECTION("timeout") {
        REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
    }
}

TEST_CASE("manual-reset event", "[atomic::event]") {
    SECTION("not set initially") {
        using namespace std::chrono_literals;

        zero::atomic::Event event{true};
        REQUIRE_FALSE(event.isSet());

        SECTION("normal") {
            std::atomic<int> n;

            std::thread thread{
                [&] {
                    std::this_thread::sleep_for(10ms);
                    n = 1;
                    event.set();
                }
            };
            Z_DEFER(thread.join());

            REQUIRE(event.wait());
            REQUIRE(event.isSet());
            REQUIRE(n == 1);

            event.reset();
            REQUIRE_FALSE(event.isSet());
        }

        SECTION("timeout") {
            REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
        }
    }

    SECTION("initial set") {
        zero::atomic::Event event{true, true};
        REQUIRE(event.isSet());

        REQUIRE(event.wait());
        REQUIRE(event.isSet());

        event.reset();
        REQUIRE_FALSE(event.isSet());
    }
}
