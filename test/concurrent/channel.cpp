#include <thread>
#include <zero/concurrent/channel.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("channel error condition", "[concurrent]") {
    const std::error_condition condition = zero::concurrent::ChannelError::DISCONNECTED;
    REQUIRE(condition == zero::concurrent::TrySendError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::SendError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::TryReceiveError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::ReceiveError::DISCONNECTED);
}

TEST_CASE("channel errors", "[concurrent]") {
    auto [sender, receiver] = zero::concurrent::channel<int>(5);
    REQUIRE(sender.capacity() == 5);
    REQUIRE(sender.empty());
    REQUIRE_FALSE(sender.full());
    REQUIRE_FALSE(sender.closed());

    SECTION("try receive") {
        const auto result = receiver.tryReceive();
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::concurrent::TryReceiveError::EMPTY);
    }

    SECTION("try send") {
        REQUIRE(sender.trySend(0));
        REQUIRE(sender.trySend(1));
        REQUIRE(sender.trySend(2));
        REQUIRE(sender.trySend(3));
        REQUIRE(sender.size() == 4);
        REQUIRE(sender.full());

        const auto result = sender.trySend(4);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::concurrent::TrySendError::FULL);
    }

    SECTION("close") {
        SECTION("receive after closed") {
            SECTION("empty") {
                sender.close();
                REQUIRE(sender.empty());
                REQUIRE(sender.closed());

                const auto result = receiver.receive();
                REQUIRE_FALSE(result);
                REQUIRE(result.error() == zero::concurrent::ReceiveError::DISCONNECTED);
            }

            SECTION("no empty") {
                REQUIRE(sender.send(0));
                REQUIRE(sender.send(1));

                sender.close();
                REQUIRE(sender.size() == 2);
                REQUIRE(sender.closed());

                auto result = receiver.receive();
                REQUIRE(result);
                REQUIRE(*result == 0);

                result = receiver.receive();
                REQUIRE(result);
                REQUIRE(*result == 1);

                result = receiver.receive();
                REQUIRE_FALSE(result);
                REQUIRE(result.error() == zero::concurrent::ReceiveError::DISCONNECTED);
            }
        }

        SECTION("send after closed") {
            REQUIRE(sender.send(0));
            REQUIRE(sender.send(1));

            sender.close();
            REQUIRE(sender.size() == 2);
            REQUIRE(sender.closed());

            const auto result = sender.send(2);
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == zero::concurrent::SendError::DISCONNECTED);
        }
    }

    SECTION("receive timeout") {
        using namespace std::chrono_literals;
        const auto result = receiver.receive(50ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::concurrent::ReceiveError::TIMEOUT);
    }

    SECTION("send timeout") {
        using namespace std::chrono_literals;

        REQUIRE(sender.trySend(0));
        REQUIRE(sender.trySend(1));
        REQUIRE(sender.trySend(2));
        REQUIRE(sender.trySend(3));
        REQUIRE(sender.size() == 4);
        REQUIRE(sender.full());

        const auto result = sender.send(4, 50ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == zero::concurrent::SendError::TIMEOUT);
    }
}

TEST_CASE("receiver disconnect", "[concurrent]") {
    auto [sender, receiver] = zero::concurrent::channel<int>(5);
    REQUIRE_FALSE(sender.closed());

    std::thread thread{
        [](zero::concurrent::Receiver<int> r) {
            const auto result = r.receive();
            assert(result);
            assert(*result == 0);
        },
        std::move(receiver)
    };

    REQUIRE(sender.trySend(0));
    thread.join();
    REQUIRE(sender.closed());

    const auto result = sender.send(2);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == zero::concurrent::SendError::DISCONNECTED);
}

TEST_CASE("sender disconnect", "[concurrent]") {
    auto [sender, receiver] = zero::concurrent::channel<int>(5);
    REQUIRE_FALSE(receiver.closed());

    std::thread thread{
        [](zero::concurrent::Sender<int> s) {
            const auto result = s.trySend(0);
            assert(result);
        },
        std::move(sender)
    };

    auto result = receiver.receive();
    REQUIRE(result);
    REQUIRE(*result == 0);

    thread.join();
    REQUIRE(receiver.closed());

    result = receiver.receive();
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == zero::concurrent::ReceiveError::DISCONNECTED);
}

TEST_CASE("channel concurrency testing", "[concurrent]") {
    std::array<std::atomic<int>, 2> counters;
    auto [sender, receiver] = zero::concurrent::channel<int>(5);

    auto produce = [&] {
        while (true) {
            if (counters[0] > 100000)
                break;

            const auto result = sender.send(counters[0]++);
            assert(result);
        }
    };

    auto consume = [&] {
        while (true) {
            if (const auto result = receiver.receive(); !result) {
                assert(result.error() == zero::concurrent::ReceiveError::DISCONNECTED);
                break;
            }

            ++counters[1];
        }
    };

    std::array producers{
        std::thread{produce},
        std::thread{produce},
        std::thread{produce},
        std::thread{produce},
        std::thread{produce}
    };

    std::array consumers{
        std::thread{consume},
        std::thread{consume},
        std::thread{consume},
        std::thread{consume},
        std::thread{consume}
    };

    for (auto &producer: producers)
        producer.join();

    sender.close();

    for (auto &consumer: consumers)
        consumer.join();

    REQUIRE(counters[0] == counters[1]);
}
