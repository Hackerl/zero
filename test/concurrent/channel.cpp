#include <thread>
#include <zero/concurrent/channel.h>
#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("atomic channel", "[concurrent]") {
    SECTION("error handling") {
        zero::concurrent::Channel<int> channel(5);

        SECTION("try receive") {
            const auto result = channel.tryReceive();
            REQUIRE(!result);
            REQUIRE(result.error() == zero::concurrent::ChannelError::EMPTY);
            REQUIRE(result.error() == std::errc::operation_would_block);
        }

        SECTION("try send") {
            auto result = channel.trySend(0);
            REQUIRE(result);

            result = channel.trySend(1);
            REQUIRE(result);

            result = channel.trySend(2);
            REQUIRE(result);

            result = channel.trySend(3);
            REQUIRE(result);

            result = channel.trySend(4);
            REQUIRE(!result);
            REQUIRE(result.error() == zero::concurrent::ChannelError::FULL);
            REQUIRE(result.error() == std::errc::operation_would_block);
        }

        SECTION("close") {
            SECTION("receive after closed") {
                SECTION("empty") {
                    channel.close();
                    const auto result = channel.receive();
                    REQUIRE(!result);
                    REQUIRE(result.error() == zero::concurrent::ChannelError::CHANNEL_EOF);
                }

                SECTION("no empty") {
                    channel.send(0);
                    channel.send(1);
                    channel.close();

                    auto result = channel.receive();
                    REQUIRE(result);
                    REQUIRE(*result == 0);

                    result = channel.receive();
                    REQUIRE(result);
                    REQUIRE(*result == 1);

                    result = channel.receive();
                    REQUIRE(!result);
                    REQUIRE(result.error() == zero::concurrent::ChannelError::CHANNEL_EOF);
                }
            }

            SECTION("send after closed") {
                channel.send(0);
                channel.send(1);
                channel.close();
                const auto result = channel.send(2);
                REQUIRE(!result);
                REQUIRE(result.error() == std::errc::broken_pipe);
            }
        }

        SECTION("receive timeout") {
            const auto result = channel.receive(50ms);
            REQUIRE(!result);
            REQUIRE(result.error() == zero::concurrent::ChannelError::RECEIVE_TIMEOUT);
            REQUIRE(result.error() == std::errc::timed_out);
        }

        SECTION("send timeout") {
            auto result = channel.send(0);
            REQUIRE(result);

            result = channel.send(1);
            REQUIRE(result);

            result = channel.send(2);
            REQUIRE(result);

            result = channel.send(3);
            REQUIRE(result);

            result = channel.send(4, 50ms);
            REQUIRE(!result);
            REQUIRE(result.error() == zero::concurrent::ChannelError::SEND_TIMEOUT);
            REQUIRE(result.error() == std::errc::timed_out);
        }
    }

    SECTION("concurrent") {
        std::atomic<int> counters[2] = {};
        zero::concurrent::Channel<int> channel(100);

        auto produce = [&] {
            while (true) {
                if (counters[0] > 100000)
                    break;

                const auto result = channel.send(counters[0]++);
                assert(result);
            }
        };

        auto consume = [&] {
            while (true) {
                if (const auto result = channel.receive(); !result) {
                    assert(result.error() == zero::concurrent::ChannelError::CHANNEL_EOF);
                    break;
                }

                ++counters[1];
            }
        };

        std::array producers = {
            std::thread(produce),
            std::thread(produce),
            std::thread(produce),
            std::thread(produce),
            std::thread(produce)
        };

        std::array consumers = {
            std::thread(consume),
            std::thread(consume),
            std::thread(consume),
            std::thread(consume),
            std::thread(consume)
        };

        for (auto &producer: producers)
            producer.join();

        channel.close();

        for (auto &consumer: consumers)
            consumer.join();

        REQUIRE(counters[0] == counters[1]);
    }
}
