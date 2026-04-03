#include <catch_extensions.h>
#include <zero/async/promise.h>
#include <zero/concurrent/channel.h>
#include <thread>
#include <list>

namespace {
    constexpr auto ThreadNumber = 16;
    constexpr auto ChannelCapacity = 32;

    class ThreadPool {
    public:
        ThreadPool(const std::size_t number, const std::size_t capacity)
            : mChannel{zero::concurrent::channel<std::function<void()>>(capacity)} {
            for (int i{0}; i < number; i++)
                mThreads.emplace_back(&ThreadPool::dispatch, this);
        }

        ~ThreadPool() {
            mChannel.first.close();

            for (auto &thread: mThreads)
                thread.join();
        }

        void dispatch() {
            while (true) {
                const auto task = mChannel.second.receive();

                if (!task)
                    break;

                (*task)();
            }
        }

        void post(std::function<void()> f) {
            zero::error::guard(mChannel.first.send(std::move(f)));
        }

    private:
        zero::concurrent::Channel<std::function<void()>> mChannel;
        std::list<std::thread> mThreads;
    };

    ThreadPool pool{ThreadNumber, ChannelCapacity};
}

template<typename T, typename E, typename U>
zero::async::promise::Future<T, E> reject(U &&error) {
    auto promise = std::make_shared<zero::async::promise::Promise<T, E>>();
    auto future = promise->getFuture();

    pool.post([promise = std::move(promise), error = std::forward<U>(error)] {
        std::this_thread::yield();
        promise->reject(error);
    });

    return future;
}

template<typename T, typename E>
    requires std::is_void_v<T>
zero::async::promise::Future<T, E> resolve() {
    auto promise = std::make_shared<zero::async::promise::Promise<T, E>>();
    auto future = promise->getFuture();

    pool.post([promise = std::move(promise)] {
        std::this_thread::yield();
        promise->resolve();
    });

    return future;
}

template<typename T, typename E, typename U>
    requires (!std::is_void_v<T>)
zero::async::promise::Future<T, E> resolve(U &&result) {
    auto promise = std::make_shared<zero::async::promise::Promise<T, E>>();
    auto future = promise->getFuture();

    pool.post(
        [promise = std::move(promise), result = std::make_shared<std::decay_t<U>>(std::forward<U>(result))] {
            std::this_thread::yield();
            promise->resolve(std::move(*result));
        }
    );

    return future;
}

TEST_CASE("promise and future", "[async::promise]") {
    SECTION("void") {
        zero::async::promise::Promise<void, std::error_code> promise;
        REQUIRE(promise.valid());
        REQUIRE_FALSE(promise.isFulfilled());

        auto future = promise.getFuture();
        REQUIRE(future.valid());
        REQUIRE_FALSE(future.isReady());

        SECTION("resolve") {
            promise.resolve();
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());
            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());
            REQUIRE(future.result());
        }

        SECTION("reject") {
            promise.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());
            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }

    SECTION("not void") {
        zero::async::promise::Promise<int, std::error_code> promise;
        REQUIRE(promise.valid());
        REQUIRE_FALSE(promise.isFulfilled());

        auto future = promise.getFuture();
        REQUIRE(future.valid());
        REQUIRE_FALSE(future.isReady());

        SECTION("resolve") {
            promise.resolve(1);
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());
            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());
            REQUIRE(future.result() == 1);
        }

        SECTION("reject") {
            promise.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());
            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }
}

TEST_CASE("create a resolved promise", "[async::promise]") {
    SECTION("void") {
        const auto future = zero::async::promise::resolve<void, std::error_code>();
        REQUIRE(future.isReady());
        REQUIRE(future.result());
    }

    SECTION("not void") {
        const auto future = zero::async::promise::resolve<int, std::error_code>(1);
        REQUIRE(future.isReady());
        REQUIRE(future.result() == 1);
    }
}

TEST_CASE("create a rejected promise", "[async::promise]") {
    const auto future = zero::async::promise::reject<void, std::error_code>(
        make_error_code(std::errc::invalid_argument)
    );
    REQUIRE(future.isReady());
    REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
}

TEST_CASE("receive future value", "[async::promise]") {
    SECTION("void") {
        zero::async::promise::resolve<void, std::error_code>()
            .then([] {
                SUCCEED();
            });
    }

    SECTION("not void") {
        zero::async::promise::resolve<int, std::error_code>(1)
            .then([](const auto &value) {
                REQUIRE(value == 1);
            });
    }
}

TEST_CASE("receive future value and transform", "[async::promise]") {
    SECTION("void") {
        const auto result = zero::async::promise::resolve<void, std::error_code>()
                            .then([] {
                                return 1;
                            })
                            .get();
        REQUIRE(result == 1);
    }

    SECTION("not void") {
        const auto result = zero::async::promise::resolve<int, std::error_code>(1)
                            .then([](const auto &value) {
                                return value * 2;
                            })
                            .get();
        REQUIRE(result == 2);
    }
}

TEST_CASE("receive future value and try to do something", "[async::promise]") {
    SECTION("void") {
        SECTION("success") {
            const auto result = zero::async::promise::resolve<void, std::error_code>()
                                .then([]() -> std::expected<int, std::error_code> {
                                    return 1;
                                })
                                .get();
            REQUIRE(result == 1);
        }

        SECTION("failure") {
            const auto result = zero::async::promise::resolve<void, std::error_code>()
                                .then([]() -> std::expected<int, std::error_code> {
                                    return std::unexpected{make_error_code(std::errc::invalid_argument)};
                                })
                                .get();
            REQUIRE_ERROR(result, std::errc::invalid_argument);
        }
    }

    SECTION("not void") {
        SECTION("success") {
            const auto result = zero::async::promise::resolve<int, std::error_code>(1)
                                .then([](const auto &value) -> std::expected<int, std::error_code> {
                                    return value * 2;
                                })
                                .get();
            REQUIRE(result == 2);
        }

        SECTION("failure") {
            const auto result = zero::async::promise::resolve<int, std::error_code>(1)
                                .then([](const auto &) -> std::expected<int, std::error_code> {
                                    return std::unexpected{make_error_code(std::errc::invalid_argument)};
                                })
                                .get();
            REQUIRE_ERROR(result, std::errc::invalid_argument);
        }
    }
}

TEST_CASE("receive future value and try to do something asynchronously", "[async::promise]") {
    SECTION("void") {
        SECTION("success") {
            const auto result = zero::async::promise::resolve<void, std::error_code>()
                                .then([] {
                                    return zero::async::promise::resolve<int, std::error_code>(1);
                                })
                                .get();
            REQUIRE(result == 1);
        }

        SECTION("failure") {
            const auto result = zero::async::promise::resolve<void, std::error_code>()
                                .then([] {
                                    return zero::async::promise::reject<int, std::error_code>(
                                        make_error_code(std::errc::invalid_argument)
                                    );
                                })
                                .get();
            REQUIRE_ERROR(result, std::errc::invalid_argument);
        }
    }

    SECTION("not void") {
        SECTION("success") {
            const auto result = zero::async::promise::resolve<int, std::error_code>(1)
                                .then([](const auto &value) {
                                    return zero::async::promise::resolve<int, std::error_code>(value * 2);
                                })
                                .get();
            REQUIRE(result == 2);
        }

        SECTION("failure") {
            const auto result = zero::async::promise::resolve<int, std::error_code>(1)
                                .then([](const auto &) {
                                    return zero::async::promise::reject<int, std::error_code>(
                                        make_error_code(std::errc::invalid_argument)
                                    );
                                })
                                .get();
            REQUIRE_ERROR(result, std::errc::invalid_argument);
        }
    }
}

TEST_CASE("catch future error", "[async::promise]") {
    zero::async::promise::reject<void, std::error_code>(make_error_code(std::errc::invalid_argument))
        .fail([](const auto &error) {
            REQUIRE(error == std::errc::invalid_argument);
        });
}

TEST_CASE("catch future error and fallback", "[async::promise]") {
    const auto result = zero::async::promise::reject<int, std::error_code>(make_error_code(std::errc::invalid_argument))
                        .fail([](const auto &) {
                            return 1;
                        })
                        .get();
    REQUIRE(result == 1);
}

TEST_CASE("catch future error and transform", "[async::promise]") {
    const auto result = zero::async::promise::reject<void, std::error_code>(
                            make_error_code(std::errc::invalid_argument)
                        )
                        .fail([](const auto &error) {
                            return std::unexpected{error.value()};
                        })
                        .get();
    REQUIRE_ERROR(result, std::to_underlying(std::errc::invalid_argument));
}

TEST_CASE("catch future error and do something", "[async::promise]") {
    SECTION("success") {
        const auto result = zero::async::promise::reject<int, std::error_code>(
                                make_error_code(std::errc::invalid_argument)
                            )
                            .fail([](const auto &) -> std::expected<int, std::error_code> {
                                return 1;
                            })
                            .get();
        REQUIRE(result == 1);
    }

    SECTION("failure") {
        const auto result = zero::async::promise::reject<int, std::error_code>(
                                make_error_code(std::errc::invalid_argument)
                            )
                            .fail([](const auto &) -> std::expected<int, std::error_code> {
                                return std::unexpected{make_error_code(std::errc::io_error)};
                            })
                            .get();
        REQUIRE_ERROR(result, std::errc::io_error);
    }
}

TEST_CASE("catch future error and do something asynchronously", "[async::promise]") {
    SECTION("success") {
        const auto result = zero::async::promise::reject<int, std::error_code>(
                                make_error_code(std::errc::invalid_argument)
                            )
                            .fail([](const auto &) {
                                return zero::async::promise::resolve<int, std::error_code>(1);
                            })
                            .get();
        REQUIRE(result == 1);
    }

    SECTION("failure") {
        const auto result = zero::async::promise::reject<int, std::error_code>(
                                make_error_code(std::errc::invalid_argument)
                            )
                            .fail([](const auto &) {
                                return zero::async::promise::reject<int, std::error_code>(
                                    make_error_code(std::errc::io_error));
                            })
                            .get();
        REQUIRE_ERROR(result, std::errc::io_error);
    }
}

TEST_CASE("handle future result", "[async::promise]") {
    SECTION("void") {
        SECTION("success") {
            zero::async::promise::resolve<void, std::error_code>()
                .then(
                    [] {
                        SUCCEED();
                    },
                    [](const auto &) {
                        FAIL();
                    }
                );
        }

        SECTION("failure") {
            zero::async::promise::reject<void, std::error_code>(make_error_code(std::errc::invalid_argument))
                .then(
                    [] {
                        FAIL();
                    },
                    [](const auto &error) {
                        REQUIRE(error == std::errc::invalid_argument);
                    }
                );
        }
    }

    SECTION("not void") {
        SECTION("success") {
            zero::async::promise::resolve<int, std::error_code>(1)
                .then(
                    [](const auto &value) {
                        REQUIRE(value == 1);
                    },
                    [](const auto &) {
                        FAIL();
                    }
                );
        }

        SECTION("failure") {
            zero::async::promise::reject<int, std::error_code>(make_error_code(std::errc::invalid_argument))
                .then(
                    [](const auto &) {
                        FAIL();
                    },
                    [](const auto &error) {
                        REQUIRE(error == std::errc::invalid_argument);
                    }
                );
        }
    }
}

TEST_CASE("future finally callback", "[async::promise]") {
    SECTION("void") {
        SECTION("success") {
            const auto result = zero::async::promise::resolve<void, std::error_code>()
                                .finally([] {
                                    SUCCEED();
                                })
                                .get();
            REQUIRE(result);
        }

        SECTION("failure") {
            const auto result = zero::async::promise::reject<void, std::error_code>(
                                    make_error_code(std::errc::invalid_argument)
                                )
                                .finally([] {
                                    SUCCEED();
                                })
                                .get();
            REQUIRE_ERROR(result, std::errc::invalid_argument);
        }
    }

    SECTION("not void") {
        SECTION("success") {
            const auto result = zero::async::promise::resolve<int, std::error_code>(1)
                                .finally([] {
                                    SUCCEED();
                                })
                                .get();
            REQUIRE(result == 1);
        }

        SECTION("failure") {
            const auto result = zero::async::promise::reject<int, std::error_code>(
                                    make_error_code(std::errc::invalid_argument)
                                )
                                .finally([] {
                                    SUCCEED();
                                })
                                .get();
            REQUIRE_ERROR(result, std::errc::invalid_argument);
        }
    }
}

TEST_CASE("promise all", "[async::promise]") {
    SECTION("void") {
        zero::async::promise::Promise<void, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;

        const auto future = all(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        SECTION("success") {
            promise1.resolve();
            promise2.resolve();
            REQUIRE(future.wait());
            REQUIRE(future.result());
        }

        SECTION("failure") {
            promise1.resolve();
            promise2.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }

    SECTION("not void") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<int, std::error_code> promise2;

        const auto future = all(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        SECTION("success") {
            promise1.resolve(1);
            promise2.resolve(2);
            REQUIRE(future.wait());

            const auto result = future.result();
            REQUIRE(result);
            REQUIRE(result->at(0) == 1);
            REQUIRE(result->at(1) == 2);
        }

        SECTION("failure") {
            promise1.resolve(1);
            promise2.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }
}

TEST_CASE("promise variadic all", "[async::promise]") {
    SECTION("same types") {
        SECTION("void") {
            zero::async::promise::Promise<void, std::error_code> promise1;
            zero::async::promise::Promise<void, std::error_code> promise2;

            const auto future = all(
                promise1.getFuture(),
                promise2.getFuture()
            );

            SECTION("success") {
                promise1.resolve();
                promise2.resolve();
                REQUIRE(future.wait());
                REQUIRE(future.result());
            }

            SECTION("failure") {
                promise1.resolve();
                promise2.reject(make_error_code(std::errc::invalid_argument));
                REQUIRE(future.wait());
                REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
            }
        }

        SECTION("not void") {
            zero::async::promise::Promise<int, std::error_code> promise1;
            zero::async::promise::Promise<int, std::error_code> promise2;

            const auto future = all(
                promise1.getFuture(),
                promise2.getFuture()
            );

            SECTION("success") {
                promise1.resolve(1);
                promise2.resolve(2);
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE(result);
                REQUIRE(result->at(0) == 1);
                REQUIRE(result->at(1) == 2);
            }

            SECTION("failure") {
                promise1.resolve(1);
                promise2.reject(make_error_code(std::errc::invalid_argument));
                REQUIRE(future.wait());
                REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
            }
        }
    }

    SECTION("different types") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;
        zero::async::promise::Promise<long, std::error_code> promise3;

        const auto future = all(
            promise1.getFuture(),
            promise2.getFuture(),
            promise3.getFuture()
        );

        SECTION("success") {
            promise1.resolve(1);
            promise2.resolve();
            promise3.resolve(2);
            REQUIRE(future.wait());

            const auto result = future.result();
            REQUIRE(result);
            REQUIRE(std::get<0>(*result) == 1);
            REQUIRE(std::get<2>(*result) == 2);
        }

        SECTION("failure") {
            promise1.resolve(1);
            promise2.resolve();
            promise3.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }
}

TEST_CASE("promise allSettled", "[async::promise]") {
    SECTION("void") {
        zero::async::promise::Promise<void, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;

        const auto future = allSettled(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        promise1.resolve();
        promise2.reject(make_error_code(std::errc::invalid_argument));
        REQUIRE(future.wait());

        const auto result = *future.result();
        REQUIRE(result[0]);
        REQUIRE_ERROR(result[1], std::errc::invalid_argument);
    }

    SECTION("not void") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<int, std::error_code> promise2;

        const auto future = allSettled(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        promise1.resolve(1);
        promise2.reject(make_error_code(std::errc::invalid_argument));
        REQUIRE(future.wait());

        const auto result = *future.result();
        REQUIRE(result[0] == 1);
        REQUIRE_ERROR(result[1], std::errc::invalid_argument);
    }
}

TEST_CASE("promise variadic allSettled", "[async::promise]") {
    zero::async::promise::Promise<int, std::error_code> promise1;
    zero::async::promise::Promise<void, std::error_code> promise2;
    zero::async::promise::Promise<long, std::error_code> promise3;

    const auto future = allSettled(
        promise1.getFuture(),
        promise2.getFuture(),
        promise3.getFuture()
    );

    promise1.resolve(1);
    promise2.resolve();
    promise3.reject(make_error_code(std::errc::invalid_argument));
    REQUIRE(future.wait());

    const auto result = *future.result();
    REQUIRE(std::get<0>(result) == 1);
    REQUIRE(std::get<1>(result));
    REQUIRE_ERROR(std::get<2>(result), std::errc::invalid_argument);
}

TEST_CASE("promise any", "[async::promise]") {
    SECTION("void") {
        zero::async::promise::Promise<void, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;

        const auto future = any(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        SECTION("success") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            promise2.resolve();
            REQUIRE(future.wait());
            REQUIRE(future.result());
        }

        SECTION("failure") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            promise2.reject(make_error_code(std::errc::io_error));
            REQUIRE(future.wait());

            const auto result = future.result();
            REQUIRE_FALSE(result);
            REQUIRE(result.error()[0] == std::errc::invalid_argument);
            REQUIRE(result.error()[1] == std::errc::io_error);
        }
    }

    SECTION("not void") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<int, std::error_code> promise2;

        const auto future = any(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        SECTION("success") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            promise2.resolve(1);
            REQUIRE(future.wait());
            REQUIRE(future.result() == 1);
        }

        SECTION("failure") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            promise2.reject(make_error_code(std::errc::io_error));
            REQUIRE(future.wait());

            const auto result = future.result();
            REQUIRE_FALSE(result);
            REQUIRE(result.error()[0] == std::errc::invalid_argument);
            REQUIRE(result.error()[1] == std::errc::io_error);
        }
    }
}

TEST_CASE("promise variadic any", "[async::promise]") {
    SECTION("same types") {
        SECTION("void") {
            zero::async::promise::Promise<void, std::error_code> promise1;
            zero::async::promise::Promise<void, std::error_code> promise2;

            const auto future = any(
                promise1.getFuture(),
                promise2.getFuture()
            );

            SECTION("success") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                promise2.resolve();
                REQUIRE(future.wait());
                REQUIRE(future.result());
            }

            SECTION("failure") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                promise2.reject(make_error_code(std::errc::io_error));
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE_FALSE(result);
                REQUIRE(result.error()[0] == std::errc::invalid_argument);
                REQUIRE(result.error()[1] == std::errc::io_error);
            }
        }

        SECTION("not void") {
            zero::async::promise::Promise<int, std::error_code> promise1;
            zero::async::promise::Promise<int, std::error_code> promise2;

            const auto future = any(
                promise1.getFuture(),
                promise2.getFuture()
            );

            SECTION("success") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                promise2.resolve(1);
                REQUIRE(future.wait());
                REQUIRE(future.result() == 1);
            }

            SECTION("failure") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                promise2.reject(make_error_code(std::errc::io_error));
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE_FALSE(result);
                REQUIRE(result.error()[0] == std::errc::invalid_argument);
                REQUIRE(result.error()[1] == std::errc::io_error);
            }
        }
    }

#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 190000
    SECTION("different types") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;
        zero::async::promise::Promise<long, std::error_code> promise3;

        const auto future = any(
            promise1.getFuture(),
            promise2.getFuture(),
            promise3.getFuture()
        );

        SECTION("success") {
            SECTION("no value") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                promise2.resolve();
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE(result);
                REQUIRE_FALSE(result->has_value());
            }

            SECTION("has value") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                promise2.reject(make_error_code(std::errc::io_error));
                promise3.resolve(1);
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE(result);
                REQUIRE(result->has_value());

#if defined(_CPPRTTI) || defined(__GXX_RTTI)
                REQUIRE(result->type() == typeid(long));
#endif
                REQUIRE(std::any_cast<long>(*result) == 1);
            }
        }

        SECTION("failure") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            promise2.reject(make_error_code(std::errc::io_error));
            promise3.reject(make_error_code(std::errc::bad_message));
            REQUIRE(future.wait());

            const auto result = future.result();
            REQUIRE_FALSE(result);
            REQUIRE(result.error()[0] == std::errc::invalid_argument);
            REQUIRE(result.error()[1] == std::errc::io_error);
            REQUIRE(result.error()[2] == std::errc::bad_message);
        }
    }
#endif
}

TEST_CASE("promise race", "[async::promise]") {
    SECTION("void") {
        zero::async::promise::Promise<void, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;

        const auto future = race(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        SECTION("success") {
            promise1.resolve();
            REQUIRE(future.wait());
            REQUIRE(future.result());
        }

        SECTION("failure") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }

    SECTION("not void") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<int, std::error_code> promise2;

        const auto future = race(std::array{
            promise1.getFuture(),
            promise2.getFuture()
        });

        SECTION("success") {
            promise1.resolve(1);
            REQUIRE(future.wait());
            REQUIRE(future.result() == 1);
        }

        SECTION("failure") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }
}

TEST_CASE("promise variadic race", "[async::promise]") {
    SECTION("same types") {
        SECTION("void") {
            zero::async::promise::Promise<void, std::error_code> promise1;
            zero::async::promise::Promise<void, std::error_code> promise2;

            const auto future = race(
                promise1.getFuture(),
                promise2.getFuture()
            );

            SECTION("success") {
                promise1.resolve();
                REQUIRE(future.wait());
                REQUIRE(future.result());
            }

            SECTION("failure") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                REQUIRE(future.wait());
                REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
            }
        }

        SECTION("not void") {
            zero::async::promise::Promise<int, std::error_code> promise1;
            zero::async::promise::Promise<int, std::error_code> promise2;

            const auto future = race(
                promise1.getFuture(),
                promise2.getFuture()
            );

            SECTION("success") {
                promise1.resolve(1);
                REQUIRE(future.wait());
                REQUIRE(future.result() == 1);
            }

            SECTION("failure") {
                promise1.reject(make_error_code(std::errc::invalid_argument));
                REQUIRE(future.wait());
                REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
            }
        }
    }

#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 190000
    SECTION("different types") {
        zero::async::promise::Promise<int, std::error_code> promise1;
        zero::async::promise::Promise<void, std::error_code> promise2;
        zero::async::promise::Promise<long, std::error_code> promise3;

        const auto future = race(
            promise1.getFuture(),
            promise2.getFuture(),
            promise3.getFuture()
        );

        SECTION("success") {
            SECTION("no value") {
                promise2.resolve();
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE(result);
                REQUIRE_FALSE(result->has_value());
            }

            SECTION("has value") {
                promise1.resolve(1);
                REQUIRE(future.wait());

                const auto result = future.result();
                REQUIRE(result);
                REQUIRE(result->has_value());

#if defined(_CPPRTTI) || defined(__GXX_RTTI)
                REQUIRE(result->type() == typeid(int));
#endif
                REQUIRE(std::any_cast<int>(*result) == 1);
            }
        }

        SECTION("failure") {
            promise1.reject(make_error_code(std::errc::invalid_argument));
            REQUIRE(future.wait());
            REQUIRE_ERROR(future.result(), std::errc::invalid_argument);
        }
    }
#endif
}

TEST_CASE("promise concurrency testing", "[async::promise]") {
    SECTION("success") {
        const auto result = resolve<int, std::error_code>(1)
                            .then([](const auto &value) {
                                return value * 2;
                            })
                            .get();
        REQUIRE(result == 2);
    }

    SECTION("failure") {
        const auto result = reject<int, std::error_code>(make_error_code(std::errc::invalid_argument))
                            .fail([](const auto &error) {
                                return std::unexpected{error.value()};
                            })
                            .get();
        REQUIRE_ERROR(result, std::to_underlying(std::errc::invalid_argument));
    }
}

TEST_CASE("promise all concurrency testing", "[async::promise]") {
    SECTION("success") {
        const auto result = all(std::array{
            resolve<int, std::error_code>(1),
            resolve<int, std::error_code>(2)
        }).get();
        REQUIRE(result);
        REQUIRE(result->at(0) == 1);
        REQUIRE(result->at(1) == 2);
    }

    SECTION("failure") {
        const auto result = all(std::array{
            resolve<int, std::error_code>(1),
            reject<int, std::error_code>(make_error_code(std::errc::invalid_argument))
        }).get();
        REQUIRE_ERROR(result, std::errc::invalid_argument);
    }
}

TEST_CASE("promise variadic all concurrency testing", "[async::promise]") {
    SECTION("success") {
        const auto result = all(
            resolve<int, std::error_code>(1),
            resolve<void, std::error_code>(),
            resolve<long, std::error_code>(2)
        ).get();
        REQUIRE(result);
        REQUIRE(std::get<0>(*result) == 1);
        REQUIRE(std::get<2>(*result) == 2);
    }

    SECTION("failure") {
        const auto result = all(
            resolve<int, std::error_code>(1),
            resolve<void, std::error_code>(),
            reject<long, std::error_code>(make_error_code(std::errc::invalid_argument))
        ).get();
        REQUIRE_ERROR(result, std::errc::invalid_argument);
    }
}

TEST_CASE("promise allSettled concurrency testing", "[async::promise]") {
    const auto result = allSettled(std::array{
        resolve<int, std::error_code>(1),
        reject<int, std::error_code>(make_error_code(std::errc::invalid_argument))
    }).get();
    REQUIRE(result[0] == 1);
    REQUIRE_ERROR(result[1], std::errc::invalid_argument);
}

TEST_CASE("promise variadic allSettled concurrency testing", "[async::promise]") {
    const auto result = allSettled(
        resolve<int, std::error_code>(1),
        resolve<void, std::error_code>(),
        reject<long, std::error_code>(make_error_code(std::errc::invalid_argument))
    ).get();
    REQUIRE(std::get<0>(result) == 1);
    REQUIRE(std::get<1>(result));
    REQUIRE_ERROR(std::get<2>(result), std::errc::invalid_argument);
}

TEST_CASE("promise any concurrency testing", "[async::promise]") {
    SECTION("success") {
        const auto result = any(std::array{
            reject<int, std::error_code>(make_error_code(std::errc::invalid_argument)),
            resolve<int, std::error_code>(1),
        }).get();
        REQUIRE(result == 1);
    }

    SECTION("failure") {
        const auto result = any(std::array{
            reject<int, std::error_code>(make_error_code(std::errc::invalid_argument)),
            reject<int, std::error_code>(make_error_code(std::errc::io_error))
        }).get();
        REQUIRE_FALSE(result);
        REQUIRE(result.error()[0] == std::errc::invalid_argument);
        REQUIRE(result.error()[1] == std::errc::io_error);
    }
}

#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 190000
TEST_CASE("promise variadic any concurrency testing", "[async::promise]") {
    SECTION("success") {
        const auto result = any(
            reject<int, std::error_code>(make_error_code(std::errc::invalid_argument)),
            reject<void, std::error_code>(make_error_code(std::errc::io_error)),
            resolve<long, std::error_code>(1)
        ).get();
        REQUIRE(result);
        REQUIRE(result->has_value());

#if defined(_CPPRTTI) || defined(__GXX_RTTI)
        REQUIRE(result->type() == typeid(long));
#endif
        REQUIRE(std::any_cast<long>(*result) == 1);
    }

    SECTION("failure") {
        const auto result = any(
            reject<int, std::error_code>(make_error_code(std::errc::invalid_argument)),
            reject<void, std::error_code>(make_error_code(std::errc::io_error)),
            reject<long, std::error_code>(make_error_code(std::errc::bad_message))
        ).get();
        REQUIRE_FALSE(result);
        REQUIRE(result.error()[0] == std::errc::invalid_argument);
        REQUIRE(result.error()[1] == std::errc::io_error);
        REQUIRE(result.error()[2] == std::errc::bad_message);
    }
}
#endif

TEST_CASE("promise race concurrency testing", "[async::promise]") {
    zero::async::promise::Promise<int, std::error_code> promise;

    SECTION("success") {
        const auto result = race(std::array{
            promise.getFuture(),
            resolve<int, std::error_code>(1)
        }).get();
        REQUIRE(result == 1);
    }

    SECTION("failure") {
        const auto result = race(std::array{
            promise.getFuture(),
            reject<int, std::error_code>(make_error_code(std::errc::invalid_argument))
        }).get();
        REQUIRE_ERROR(result, std::errc::invalid_argument);
    }
}

#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 190000
TEST_CASE("promise variadic race concurrency testing", "[async::promise]") {
    zero::async::promise::Promise<int, std::error_code> promise1;
    zero::async::promise::Promise<void, std::error_code> promise2;

    SECTION("success") {
        const auto result = race(
            promise1.getFuture(),
            promise2.getFuture(),
            resolve<long, std::error_code>(1)
        ).get();
        REQUIRE(result);
        REQUIRE(result->has_value());

#if defined(_CPPRTTI) || defined(__GXX_RTTI)
        REQUIRE(result->type() == typeid(long));
#endif
        REQUIRE(std::any_cast<long>(*result) == 1);
    }

    SECTION("failure") {
        const auto result = race(
            promise1.getFuture(),
            promise2.getFuture(),
            reject<long, std::error_code>(make_error_code(std::errc::invalid_argument))
        ).get();
        REQUIRE_ERROR(result, std::errc::invalid_argument);
    }
}
#endif
