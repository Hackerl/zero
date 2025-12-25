#include <catch_extensions.h>
#include <zero/expect.h>
#include <zero/concurrent/channel.h>
#include <future>

TEST_CASE("channel error condition", "[concurrent::channel]") {
    const std::error_condition condition{zero::concurrent::ChannelError::DISCONNECTED};
    REQUIRE(condition == zero::concurrent::TrySendError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::SendError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::TryReceiveError::DISCONNECTED);
    REQUIRE(condition == zero::concurrent::ReceiveError::DISCONNECTED);
}

TEST_CASE("channel try send error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::TrySendError::DISCONNECTED};
        REQUIRE(ec == zero::concurrent::ChannelError::DISCONNECTED);
    }

    SECTION("full") {
        const std::error_code ec{zero::concurrent::TrySendError::FULL};
        REQUIRE(ec == std::errc::operation_would_block);
    }
}

TEST_CASE("channel send error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::SendError::DISCONNECTED};
        REQUIRE(ec == zero::concurrent::ChannelError::DISCONNECTED);
    }

    SECTION("timeout") {
        const std::error_code ec{zero::concurrent::SendError::TIMEOUT};
        REQUIRE(ec == std::errc::timed_out);
    }
}

TEST_CASE("channel try receive error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::TryReceiveError::DISCONNECTED};
        REQUIRE(ec == zero::concurrent::ChannelError::DISCONNECTED);
    }

    SECTION("empty") {
        const std::error_code ec{zero::concurrent::TryReceiveError::EMPTY};
        REQUIRE(ec == std::errc::operation_would_block);
    }
}

TEST_CASE("channel receive error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::ReceiveError::DISCONNECTED};
        REQUIRE(ec == zero::concurrent::ChannelError::DISCONNECTED);
    }

    SECTION("timeout") {
        const std::error_code ec{zero::concurrent::ReceiveError::TIMEOUT};
        REQUIRE(ec == std::errc::timed_out);
    }
}

TEST_CASE("channel sender", "[concurrent::channel]") {
    const auto capacity = GENERATE(1uz, take(1, random(2uz, 1024uz)));
    const auto element = GENERATE(take(1, randomString(1, 1024)));

    auto [sender, receiver] = zero::concurrent::channel<std::string>(capacity);

    SECTION("try send") {
        SECTION("success") {
            REQUIRE(sender.trySend(element));
        }

        SECTION("disconnected") {
            sender.close();
            REQUIRE_ERROR(sender.trySend(element), zero::concurrent::TrySendError::DISCONNECTED);
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i) {
                REQUIRE(sender.trySend(element));
            }

            REQUIRE_ERROR(sender.trySend(element), zero::concurrent::TrySendError::FULL);
        }
    }

    SECTION("try send extended") {
        SECTION("success") {
            REQUIRE(sender.trySendEx(std::string{element}));
        }

        SECTION("disconnected") {
            sender.close();

            const auto result = sender.trySendEx(std::string{element});
            REQUIRE_FALSE(result);
            REQUIRE(std::get<0>(result.error()) == element);
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::TrySendError::DISCONNECTED);
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i) {
                REQUIRE(sender.trySend(element));
            }

            const auto result = sender.trySendEx(std::string{element});
            REQUIRE_FALSE(result);
            REQUIRE(std::get<0>(result.error()) == element);
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::TrySendError::FULL);
        }
    }

    SECTION("send") {
        SECTION("success") {
            SECTION("no wait") {
                REQUIRE(sender.send(element));
            }

            SECTION("wait") {
                for (std::size_t i{0}; i < capacity; ++i) {
                    REQUIRE(sender.trySend(element));
                }

                auto future = std::async([&] {
                    return sender.send(element);
                });
                REQUIRE(receiver.receive() == element);
                REQUIRE(future.get());
            }

            SECTION("wait with timeout") {
                using namespace std::chrono_literals;

                for (std::size_t i{0}; i < capacity; ++i) {
                    REQUIRE(sender.trySend(element));
                }

                auto future = std::async([&] {
                    return sender.send(element, 1s);
                });

                std::this_thread::sleep_for(10ms);
                REQUIRE(receiver.receive() == element);
                REQUIRE(future.get());
            }
        }

        SECTION("disconnected") {
            sender.close();
            REQUIRE_ERROR(sender.send(element), zero::concurrent::SendError::DISCONNECTED);
        }

        SECTION("timeout") {
            using namespace std::chrono_literals;

            for (std::size_t i{0}; i < capacity; ++i) {
                REQUIRE(sender.trySend(element));
            }

            REQUIRE_ERROR(sender.send(element, 10ms), zero::concurrent::SendError::TIMEOUT);
        }
    }

    SECTION("send extended") {
        SECTION("success") {
            SECTION("no wait") {
                REQUIRE(sender.sendEx(std::string{element}));
            }

            SECTION("wait") {
                for (std::size_t i{0}; i < capacity; ++i) {
                    REQUIRE(sender.trySend(element));
                }

                auto future = std::async([&] {
                    return sender.sendEx(std::string{element});
                });
                REQUIRE(receiver.receive() == element);
                REQUIRE(future.get());
            }

            SECTION("wait with timeout") {
                using namespace std::chrono_literals;

                for (std::size_t i{0}; i < capacity; ++i) {
                    REQUIRE(sender.trySend(element));
                }

                auto future = std::async([&] {
                    return sender.sendEx(std::string{element}, 1s);
                });

                std::this_thread::sleep_for(10ms);
                REQUIRE(receiver.receive() == element);
                REQUIRE(future.get());
            }
        }

        SECTION("disconnected") {
            sender.close();

            const auto result = sender.sendEx(std::string{element});
            REQUIRE_FALSE(result);
            REQUIRE(std::get<0>(result.error()) == element);
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::SendError::DISCONNECTED);
        }

        SECTION("timeout") {
            using namespace std::chrono_literals;

            for (std::size_t i{0}; i < capacity; ++i) {
                REQUIRE(sender.trySend(element));
            }

            const auto result = sender.sendEx(std::string{element}, 10ms);
            REQUIRE_FALSE(result);
            REQUIRE(std::get<0>(result.error()) == element);
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::SendError::TIMEOUT);
        }
    }

    SECTION("close") {
        sender.close();
        REQUIRE(sender.closed());
    }

    SECTION("size") {
        const auto size = GENERATE_REF(take(1, random(0uz, capacity)));

        for (std::size_t i{0}; i < size; ++i) {
            REQUIRE(sender.trySend(element));
        }

        REQUIRE(sender.size() == size);
    }

    SECTION("capacity") {
        REQUIRE(sender.capacity() == capacity);
    }

    SECTION("empty") {
        SECTION("empty") {
            REQUIRE(sender.empty());
        }

        SECTION("not empty") {
            REQUIRE(sender.trySend(element));
            REQUIRE_FALSE(sender.empty());
        }
    }

    SECTION("full") {
        SECTION("not full") {
            REQUIRE_FALSE(sender.full());
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i) {
                REQUIRE(sender.trySend(element));
            }

            REQUIRE(sender.full());
        }
    }

    SECTION("closed") {
        SECTION("not closed") {
            REQUIRE_FALSE(sender.closed());
        }

        SECTION("closed") {
            sender.close();
            REQUIRE(sender.closed());
        }
    }
}

TEST_CASE("channel receiver", "[concurrent::channel]") {
    const auto capacity = GENERATE(1uz, take(1, random(2uz, 1024uz)));
    const auto element = GENERATE(take(1, randomString(1, 1024)));

    auto [sender, receiver] = zero::concurrent::channel<std::string>(capacity);

    SECTION("try receive") {
        SECTION("success") {
            REQUIRE(sender.trySend(element));

            SECTION("closed") {
                sender.close();
            }

            REQUIRE(receiver.tryReceive());
        }

        SECTION("disconnected") {
            sender.close();
            REQUIRE_ERROR(receiver.tryReceive(), zero::concurrent::TryReceiveError::DISCONNECTED);
        }

        SECTION("empty") {
            REQUIRE_ERROR(receiver.tryReceive(), zero::concurrent::TryReceiveError::EMPTY);
        }
    }

    SECTION("receive") {
        SECTION("success") {
            SECTION("no wait") {
                REQUIRE(sender.trySend(element));

                SECTION("closed") {
                    sender.close();
                }

                REQUIRE(receiver.receive() == element);
            }

            SECTION("wait") {
                auto future = std::async([&] {
                    return receiver.receive();
                });
                REQUIRE(sender.trySend(element));

                SECTION("closed") {
                    sender.close();
                }

                REQUIRE(future.get() == element);
            }

            SECTION("wait with timeout") {
                using namespace std::chrono_literals;

                auto future = std::async([&] {
                    return receiver.receive(10ms);
                });

                REQUIRE(sender.trySend(element));

                SECTION("closed") {
                    sender.close();
                }

                REQUIRE(future.get() == element);
            }
        }

        SECTION("disconnected") {
            SECTION("after close") {
                sender.close();
                REQUIRE_ERROR(receiver.receive(), zero::concurrent::ReceiveError::DISCONNECTED);
            }

            SECTION("before close") {
                auto future = std::async([&] {
                    return receiver.receive();
                });
                sender.close();
                REQUIRE_ERROR(future.get(), zero::concurrent::ReceiveError::DISCONNECTED);
            }
        }

        SECTION("timeout") {
            using namespace std::chrono_literals;
            REQUIRE_ERROR(receiver.receive(10ms), zero::concurrent::ReceiveError::TIMEOUT);
        }
    }

    SECTION("size") {
        const auto size = GENERATE_REF(take(1, random(0uz, capacity)));

        for (std::size_t i{0}; i < size; ++i) {
            REQUIRE(sender.trySend(element));
        }

        REQUIRE(receiver.size() == size);
    }

    SECTION("capacity") {
        REQUIRE(receiver.capacity() == capacity);
    }

    SECTION("empty") {
        SECTION("empty") {
            REQUIRE(receiver.empty());
        }

        SECTION("not empty") {
            REQUIRE(sender.trySend(element));
            REQUIRE_FALSE(receiver.empty());
        }
    }

    SECTION("full") {
        SECTION("not full") {
            REQUIRE_FALSE(receiver.full());
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i) {
                REQUIRE(sender.trySend(element));
            }

            REQUIRE(receiver.full());
        }
    }

    SECTION("closed") {
        SECTION("not closed") {
            REQUIRE_FALSE(receiver.closed());
        }

        SECTION("closed") {
            sender.close();
            REQUIRE(receiver.closed());
        }
    }
}

TEST_CASE("channel receiver dropped", "[concurrent::channel]") {
    const auto capacity = GENERATE(take(1, random(1uz, 1024uz)));
    const auto element = GENERATE(take(1, randomString(1, 1024)));

    auto [sender, receiver] = zero::concurrent::channel<std::string>(capacity);

    auto future = std::async(
        [](zero::concurrent::Receiver<std::string> r) {
            return r.receive();
        },
        std::move(receiver)
    );

    REQUIRE(sender.trySend(element));
    REQUIRE(future.get() == element);
    REQUIRE(sender.closed());
}

TEST_CASE("channel sender dropped", "[concurrent::channel]") {
    const auto capacity = GENERATE(take(1, random(1uz, 1024uz)));
    const auto element = GENERATE(take(1, randomString(1, 1024)));

    auto [sender, receiver] = zero::concurrent::channel<std::string>(capacity);

    auto future = std::async(
        [&](zero::concurrent::Sender<std::string> s) {
            return s.trySend(element);
        },
        std::move(sender)
    );

    REQUIRE(receiver.receive() == element);
    REQUIRE(future.get());
    REQUIRE(receiver.closed());
}

TEST_CASE("channel concurrency testing", "[concurrent::channel]") {
    const auto capacity = GENERATE(take(5, random(1uz, 1024uz)));
    const auto element = GENERATE(take(1, randomString(1, 1024)));
    const auto times = GENERATE(take(5, random(1, 102400)));

    auto [sender, receiver] = zero::concurrent::channel<std::string>(capacity);

    std::atomic<int> counter;

    const auto produce = [&]() -> std::expected<void, std::error_code> {
        for (int i{0}; i < times; ++i) {
            Z_EXPECT(sender.send(element));
        }

        return {};
    };

    const auto consume = [&]() -> std::expected<void, std::error_code> {
        while (true) {
            const auto result = receiver.receive();

            if (!result) {
                if (result.error() != zero::concurrent::ReceiveError::DISCONNECTED)
                    return std::unexpected{result.error()};

                return {};
            }

            if (*result != element)
                return std::unexpected{make_error_code(std::errc::bad_message)};

            ++counter;
        }
    };

    std::array producers{std::async(produce), std::async(produce)};
    std::array consumers{std::async(consume), std::async(consume)};

    for (auto &future: producers) {
        REQUIRE(future.get());
    }

    sender.close();

    for (auto &future: consumers) {
        REQUIRE(future.get());
    }

    REQUIRE(counter == times * 2);
}
