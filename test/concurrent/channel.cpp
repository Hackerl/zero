#include <catch_extensions.h>
#include <zero/expect.h>
#include <zero/concurrent/channel.h>
#include <future>

TEST_CASE("channel error condition", "[concurrent::channel]") {
    const std::error_condition condition = zero::concurrent::ChannelError::DISCONNECTED;
    REQUIRE(condition == zero::concurrent::TrySendError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::SendError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::TryReceiveError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::ReceiveError::DISCONNECTED);
}

TEST_CASE("channel errors", "[concurrent::channel]") {
    auto [sender, receiver] = zero::concurrent::channel<int>(5);
    REQUIRE(sender.capacity() == 5);
    REQUIRE(sender.empty());
    REQUIRE_FALSE(sender.full());
    REQUIRE_FALSE(sender.closed());

    SECTION("try receive") {
        REQUIRE_ERROR(receiver.tryReceive(), zero::concurrent::TryReceiveError::EMPTY);
    }

    SECTION("try send") {
        REQUIRE(sender.trySend(0));
        REQUIRE(sender.trySend(1));
        REQUIRE(sender.trySend(2));
        REQUIRE(sender.trySend(3));
        REQUIRE(sender.size() == 4);
        REQUIRE(sender.full());
        REQUIRE_ERROR(sender.trySend(4), zero::concurrent::TrySendError::FULL);
    }

    SECTION("close") {
        SECTION("receive after closed") {
            SECTION("empty") {
                sender.close();
                REQUIRE(sender.empty());
                REQUIRE(sender.closed());
                REQUIRE_ERROR(receiver.receive(), zero::concurrent::ReceiveError::DISCONNECTED);
            }

            SECTION("no empty") {
                REQUIRE(sender.send(0));
                REQUIRE(sender.send(1));

                sender.close();
                REQUIRE(sender.size() == 2);
                REQUIRE(sender.closed());

                REQUIRE(receiver.receive() == 0);
                REQUIRE(receiver.receive() == 1);
                REQUIRE_ERROR(receiver.receive(), zero::concurrent::ReceiveError::DISCONNECTED);
            }
        }

        SECTION("send after closed") {
            REQUIRE(sender.send(0));
            REQUIRE(sender.send(1));

            sender.close();
            REQUIRE(sender.size() == 2);
            REQUIRE(sender.closed());
            REQUIRE_ERROR(sender.send(2), zero::concurrent::SendError::DISCONNECTED);
        }
    }

    SECTION("receive timeout") {
        using namespace std::chrono_literals;
        REQUIRE_ERROR(receiver.receive(50ms), zero::concurrent::ReceiveError::TIMEOUT);
    }

    SECTION("send timeout") {
        using namespace std::chrono_literals;

        REQUIRE(sender.trySend(0));
        REQUIRE(sender.trySend(1));
        REQUIRE(sender.trySend(2));
        REQUIRE(sender.trySend(3));
        REQUIRE(sender.size() == 4);
        REQUIRE(sender.full());
        REQUIRE_ERROR(sender.send(4, 50ms), zero::concurrent::SendError::TIMEOUT);
    }
}

TEST_CASE("receiver disconnect", "[concurrent::channel]") {
    auto [sender, receiver] = zero::concurrent::channel<int>(5);
    REQUIRE_FALSE(sender.closed());

    auto future = std::async(
        [](zero::concurrent::Receiver<int> r) {
            return r.receive();
        },
        std::move(receiver)
    );

    REQUIRE(sender.send(0));
    REQUIRE(future.get() == 0);
    REQUIRE(sender.closed());
    REQUIRE_ERROR(sender.send(2), zero::concurrent::SendError::DISCONNECTED);
}

TEST_CASE("sender disconnect", "[concurrent::channel]") {
    auto [sender, receiver] = zero::concurrent::channel<int>(5);
    REQUIRE_FALSE(receiver.closed());

    auto future = std::async(
        [](zero::concurrent::Sender<int> s) {
            return s.send(0);
        },
        std::move(sender)
    );

    REQUIRE(receiver.receive() == 0);
    REQUIRE(future.get());
    REQUIRE(receiver.closed());
    REQUIRE_ERROR(receiver.receive(), zero::concurrent::ReceiveError::DISCONNECTED);
}

TEST_CASE("channel concurrency testing", "[concurrent::channel]") {
    std::array<std::atomic<int>, 2> counters;
    auto [sender, receiver] = zero::concurrent::channel<int>(5);

    auto produce = [&]() -> std::expected<void, std::error_code> {
        while (true) {
            if (counters[0] > 100000)
                break;

            EXPECT(sender.send(counters[0]++));
        }

        return {};
    };

    auto consume = [&]() -> std::expected<void, std::error_code> {
        while (true) {
            EXPECT(receiver.receive());
            ++counters[1];
        }
    };

    std::array producers{
        std::async(produce),
        std::async(produce),
        std::async(produce),
        std::async(produce),
        std::async(produce)
    };

    std::array consumers{
        std::async(consume),
        std::async(consume),
        std::async(consume),
        std::async(consume),
        std::async(consume)
    };

    for (auto &future: producers) {
        REQUIRE(future.get());
    }

    sender.close();

    for (auto &future: consumers) {
        REQUIRE_ERROR(future.get(), zero::concurrent::ReceiveError::DISCONNECTED);
    }

    REQUIRE(counters[0] == counters[1]);
}
