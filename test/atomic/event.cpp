#include <catch_extensions.h>
#include <zero/atomic/event.h>
#include <zero/defer.h>
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
        REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
    }

    SECTION("timeout") {
        REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
    }
}

TEST_CASE("manual-reset event", "[atomic]") {
    using namespace std::chrono_literals;

    SECTION("not set initially") {
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
            REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
        }

        SECTION("timeout") {
            REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
        }
    }

    SECTION("initial set") {
        zero::atomic::Event event{true, true};

        REQUIRE(event.wait());
        REQUIRE(event.wait());

        event.reset();
        REQUIRE_ERROR(event.wait(10ms), std::errc::timed_out);
    }
}
