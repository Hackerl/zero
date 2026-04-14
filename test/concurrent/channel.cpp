#include <catch_extensions.h>
#include <zero/expect.h>
#include <zero/concurrent/channel.h>
#include <future>

TEST_CASE("channel error condition", "[concurrent::channel]") {
    const std::error_condition condition{zero::concurrent::ChannelError::Disconnected};
    REQUIRE(condition == zero::concurrent::TrySendError::Disconnected);
    REQUIRE(condition == zero::concurrent::SendError::Disconnected);
    REQUIRE(condition == zero::concurrent::TryReceiveError::Disconnected);
    REQUIRE(condition == zero::concurrent::ReceiveError::Disconnected);
}

TEST_CASE("channel try send error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::TrySendError::Disconnected};
        REQUIRE(ec == zero::concurrent::ChannelError::Disconnected);
    }

    SECTION("full") {
        const std::error_code ec{zero::concurrent::TrySendError::Full};
        REQUIRE(ec == std::errc::operation_would_block);
    }
}

TEST_CASE("channel send error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::SendError::Disconnected};
        REQUIRE(ec == zero::concurrent::ChannelError::Disconnected);
    }

    SECTION("timeout") {
        const std::error_code ec{zero::concurrent::SendError::Timeout};
        REQUIRE(ec == std::errc::timed_out);
    }
}

TEST_CASE("channel try receive error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::TryReceiveError::Disconnected};
        REQUIRE(ec == zero::concurrent::ChannelError::Disconnected);
    }

    SECTION("empty") {
        const std::error_code ec{zero::concurrent::TryReceiveError::Empty};
        REQUIRE(ec == std::errc::operation_would_block);
    }
}

TEST_CASE("channel receive error", "[concurrent::channel]") {
    SECTION("disconnected") {
        const std::error_code ec{zero::concurrent::ReceiveError::Disconnected};
        REQUIRE(ec == zero::concurrent::ChannelError::Disconnected);
    }

    SECTION("timeout") {
        const std::error_code ec{zero::concurrent::ReceiveError::Timeout};
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
            REQUIRE_ERROR(sender.trySend(element), zero::concurrent::TrySendError::Disconnected);
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i)
                zero::error::guard(sender.trySend(element));

            REQUIRE_ERROR(sender.trySend(element), zero::concurrent::TrySendError::Full);
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
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::TrySendError::Disconnected);
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i)
                zero::error::guard(sender.trySend(element));

            const auto result = sender.trySendEx(std::string{element});
            REQUIRE_FALSE(result);
            REQUIRE(std::get<0>(result.error()) == element);
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::TrySendError::Full);
        }
    }

    SECTION("send") {
        SECTION("success") {
            SECTION("no wait") {
                REQUIRE(sender.send(element));
            }

            SECTION("wait") {
                for (std::size_t i{0}; i < capacity; ++i)
                    zero::error::guard(sender.trySend(element));

                auto future = std::async([&] {
                    return sender.send(element);
                });
                REQUIRE(receiver.receive() == element);
                REQUIRE(future.get());
            }

            SECTION("wait with timeout") {
                using namespace std::chrono_literals;

                for (std::size_t i{0}; i < capacity; ++i)
                    zero::error::guard(sender.trySend(element));

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
            REQUIRE_ERROR(sender.send(element), zero::concurrent::SendError::Disconnected);
        }

        SECTION("timeout") {
            using namespace std::chrono_literals;

            for (std::size_t i{0}; i < capacity; ++i)
                zero::error::guard(sender.trySend(element));

            REQUIRE_ERROR(sender.send(element, 10ms), zero::concurrent::SendError::Timeout);
        }
    }

    SECTION("send extended") {
        SECTION("success") {
            SECTION("no wait") {
                REQUIRE(sender.sendEx(std::string{element}));
            }

            SECTION("wait") {
                for (std::size_t i{0}; i < capacity; ++i)
                    zero::error::guard(sender.trySend(element));

                auto future = std::async([&] {
                    return sender.sendEx(std::string{element});
                });
                REQUIRE(receiver.receive() == element);
                REQUIRE(future.get());
            }

            SECTION("wait with timeout") {
                using namespace std::chrono_literals;

                for (std::size_t i{0}; i < capacity; ++i)
                    zero::error::guard(sender.trySend(element));

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
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::SendError::Disconnected);
        }

        SECTION("timeout") {
            using namespace std::chrono_literals;

            for (std::size_t i{0}; i < capacity; ++i)
                zero::error::guard(sender.trySend(element));

            const auto result = sender.sendEx(std::string{element}, 10ms);
            REQUIRE_FALSE(result);
            REQUIRE(std::get<0>(result.error()) == element);
            REQUIRE(std::get<1>(result.error()) == zero::concurrent::SendError::Timeout);
        }
    }

    SECTION("close") {
        sender.close();
        REQUIRE(sender.closed());
    }

    SECTION("size") {
        const auto size = GENERATE_REF(take(1, random(0uz, capacity)));

        for (std::size_t i{0}; i < size; ++i)
            zero::error::guard(sender.trySend(element));

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
            zero::error::guard(sender.trySend(element));
            REQUIRE_FALSE(sender.empty());
        }
    }

    SECTION("full") {
        SECTION("not full") {
            REQUIRE_FALSE(sender.full());
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i)
                zero::error::guard(sender.trySend(element));

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
            zero::error::guard(sender.trySend(element));

            SECTION("closed") {
                sender.close();
            }

            REQUIRE(receiver.tryReceive());
        }

        SECTION("disconnected") {
            sender.close();
            REQUIRE_ERROR(receiver.tryReceive(), zero::concurrent::TryReceiveError::Disconnected);
        }

        SECTION("empty") {
            REQUIRE_ERROR(receiver.tryReceive(), zero::concurrent::TryReceiveError::Empty);
        }
    }

    SECTION("receive") {
        SECTION("success") {
            SECTION("no wait") {
                zero::error::guard(sender.trySend(element));

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
                REQUIRE_ERROR(receiver.receive(), zero::concurrent::ReceiveError::Disconnected);
            }

            SECTION("before close") {
                auto future = std::async([&] {
                    return receiver.receive();
                });
                sender.close();
                REQUIRE_ERROR(future.get(), zero::concurrent::ReceiveError::Disconnected);
            }
        }

        SECTION("timeout") {
            using namespace std::chrono_literals;
            REQUIRE_ERROR(receiver.receive(10ms), zero::concurrent::ReceiveError::Timeout);
        }
    }

    SECTION("size") {
        const auto size = GENERATE_REF(take(1, random(0uz, capacity)));

        for (std::size_t i{0}; i < size; ++i)
            zero::error::guard(sender.trySend(element));

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
            zero::error::guard(sender.trySend(element));
            REQUIRE_FALSE(receiver.empty());
        }
    }

    SECTION("full") {
        SECTION("not full") {
            REQUIRE_FALSE(receiver.full());
        }

        SECTION("full") {
            for (std::size_t i{0}; i < capacity; ++i)
                zero::error::guard(sender.trySend(element));

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

    const auto produce = [&] {
        for (int i{0}; i < times; ++i)
            zero::error::guard(sender.send(element));
    };

    const auto consume = [&] {
        while (true) {
            const auto result = receiver.receive();

            if (!result) {
                if (const auto &error = result.error(); error != zero::concurrent::ReceiveError::Disconnected)
                    throw zero::error::StacktraceError<std::system_error>{error};

                break;
            }

            if (*result != element)
                throw zero::error::StacktraceError<std::runtime_error>{"Received incorrect element"};

            ++counter;
        }
    };

    std::array producers{std::async(produce), std::async(produce)};
    std::array consumers{std::async(consume), std::async(consume)};

    for (auto &future: producers) {
        REQUIRE_NOTHROW(future.get());
    }

    sender.close();

    for (auto &future: consumers) {
        REQUIRE_NOTHROW(future.get());
    }

    REQUIRE(counter == times * 2);
}
