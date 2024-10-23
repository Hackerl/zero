#include <zero/async/promise.h>
#include <zero/concurrent/channel.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <list>

constexpr auto THREAD_NUMBER = 16;
constexpr auto CHANNEL_CAPACITY = 32;

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

    template<typename F>
    void post(F &&f) {
        mChannel.first.send(std::forward<F>(f));
    }

private:
    zero::concurrent::Channel<std::function<void()>> mChannel;
    std::list<std::thread> mThreads;
};

static ThreadPool pool(THREAD_NUMBER, CHANNEL_CAPACITY);

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

TEST_CASE("promise", "[async]") {
    SECTION("basic") {
        using namespace std::chrono_literals;

        zero::async::promise::Promise<int, int> promise;
        REQUIRE(promise.valid());
        REQUIRE_FALSE(promise.isFulfilled());

        auto future = promise.getFuture();
        REQUIRE(future.valid());
        REQUIRE_FALSE(future.isReady());

        const auto result = future.wait(10ms);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == std::errc::timed_out);

        SECTION("resolve") {
            promise.resolve(1024);
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());

            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());

            const auto &res = future.result();
            REQUIRE(res);
            REQUIRE(*res == 1024);
        }

        SECTION("reject") {
            promise.reject(-1);
            REQUIRE(promise.valid());
            REQUIRE(promise.isFulfilled());

            REQUIRE(future.valid());
            REQUIRE(future.isReady());
            REQUIRE(future.wait());

            const auto &res = future.result();
            REQUIRE_FALSE(res);
            REQUIRE(res.error() == -1);
        }
    }

    SECTION("callback chain") {
        using namespace std::string_view_literals;

        zero::async::promise::chain<int, int>([](auto p) {
            p.resolve(1);
        }).then([](const auto &value) {
            REQUIRE(value == 1);
        });

        zero::async::promise::resolve<int, int>(1)
            .then([](const auto &value) {
                REQUIRE(value == 1);
            });

        zero::async::promise::reject<void, int>(-1)
            .fail([](const auto &error) {
                REQUIRE(error == -1);
            });

        zero::async::promise::resolve<int, int>(1)
            .then([](const auto &value) {
                return zero::async::promise::resolve<int, int>(value * 10);
            }).then([](const auto &value) {
                REQUIRE(value == 10);
            });

        zero::async::promise::resolve<int, int>(1)
            .then([](const auto &value) -> std::expected<int, int> {
                if (value == 2)
                    return std::unexpected{2};

                return 2;
            }).then([](const auto &value) {
                REQUIRE(value == 2);
            });

        zero::async::promise::resolve<int, int>(1)
            .then([](const auto &value) -> std::expected<int, int> {
                if (value == 1)
                    return std::unexpected{-1};

                return 2;
            }).fail([](const auto &error) {
                REQUIRE(error == -1);
                return std::unexpected{error};
            });

        const auto i = std::make_shared<int>(0);

        zero::async::promise::resolve<int, int>(1)
            .finally([=] {
                *i = 1;
            }).then([=](const auto &value) {
                REQUIRE(*i == 1);
                REQUIRE(value == 1);
            });

        zero::async::promise::resolve<std::string, int>("hello world")
            .then(&std::string::size)
            .then([](const auto &length) {
                REQUIRE(length == 11);
            });

        struct People {
            std::string name;
            int age{};
        };

        zero::async::promise::resolve<People, int>(People{"jack", 18})
            .then([](const auto &people) {
                return people.age;
            })
            .then([](const auto &age) {
                REQUIRE(age == 18);
            });

        zero::async::promise::chain<std::unique_ptr<char[]>, int>([](auto p) {
            auto buffer = std::make_unique<char[]>(1024);

            buffer[0] = 'h';
            buffer[1] = 'e';
            buffer[2] = 'l';
            buffer[3] = 'l';
            buffer[4] = 'o';

            p.resolve(std::move(buffer));
        }).then([](const auto &buffer) {
            REQUIRE(buffer.get() == "hello"sv);
        });
    }

    SECTION("concurrent") {
        using namespace std::string_view_literals;

        {
            const auto result = resolve<int, int>(1).get();
            REQUIRE(result);
            REQUIRE(*result == 1);
        }

        {
            const auto result = reject<void, int>(-1).get();
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == -1);
        }

        {
            const auto result = resolve<int, int>(1)
                                .then([](const auto &value) {
                                    return resolve<int, int>(value * 10);
                                }).get();
            REQUIRE(result);
            REQUIRE(*result == 10);
        }

        {
            const auto result = resolve<int, int>(1)
                                .then([](const auto &value) -> std::expected<int, int> {
                                    if (value == 2)
                                        return std::unexpected{2};

                                    return 2;
                                }).get();
            REQUIRE(result);
            REQUIRE(*result == 2);
        }

        {
            const auto result = resolve<int, int>(1)
                                .then([](const auto &value) -> std::expected<int, int> {
                                    if (value == 1)
                                        return std::unexpected{-1};

                                    return 2;
                                }).get();
            REQUIRE_FALSE(result);
            REQUIRE(result.error() == -1);
        }

        {
            const auto i = std::make_shared<int>(0);
            const auto result = resolve<int, int>(1)
                                .finally([=] {
                                    *i = 1;
                                }).get();
            REQUIRE(result);
            REQUIRE(*result == 1);
            REQUIRE(*i == 1);
        }

        {
            auto buffer = std::make_unique<char[]>(1024);

            buffer[0] = 'h';
            buffer[1] = 'e';
            buffer[2] = 'l';
            buffer[3] = 'l';
            buffer[4] = 'o';

            const auto result = resolve<std::unique_ptr<char[]>, int>(std::move(buffer)).get();
            REQUIRE(result);
            REQUIRE(result->get() == "hello"sv);
        }
    }

    SECTION("promise::all") {
        SECTION("ranges") {
            SECTION("void") {
                SECTION("resolve") {
                    const auto result = all(std::array{
                        resolve<void, int>(),
                        resolve<void, int>(),
                        resolve<void, int>(),
                        resolve<void, int>(),
                        resolve<void, int>()
                    }).get();
                    REQUIRE(result);
                }

                SECTION("reject") {
                    const auto result = all(std::array{
                        resolve<void, int>(),
                        resolve<void, int>(),
                        reject<void, int>(-1),
                        resolve<void, int>(),
                        resolve<void, int>()
                    }).get();
                    REQUIRE_FALSE(result);
                    REQUIRE(result.error() == -1);
                }
            }

            SECTION("not void") {
                SECTION("resolve") {
                    const auto result = all(std::array{
                        resolve<int, int>(1),
                        resolve<int, int>(2),
                        resolve<int, int>(3),
                        resolve<int, int>(4),
                        resolve<int, int>(5)
                    }).get();
                    REQUIRE(result);

                    REQUIRE(result->at(0) == 1);
                    REQUIRE(result->at(1) == 2);
                    REQUIRE(result->at(2) == 3);
                    REQUIRE(result->at(3) == 4);
                    REQUIRE(result->at(4) == 5);
                }

                SECTION("reject") {
                    const auto result = all(std::array{
                        resolve<int, int>(1),
                        resolve<int, int>(2),
                        reject<int, int>(-1),
                        resolve<int, int>(4),
                        resolve<int, int>(5)
                    }).get();
                    REQUIRE_FALSE(result);
                    REQUIRE(result.error() == -1);
                }
            }
        }

        SECTION("variadic") {
            SECTION("same types") {
                SECTION("void") {
                    SECTION("resolve") {
                        const auto result = all(
                            resolve<void, int>(),
                            resolve<void, int>(),
                            resolve<void, int>(),
                            resolve<void, int>(),
                            resolve<void, int>()
                        ).get();
                        REQUIRE(result);
                    }

                    SECTION("reject") {
                        const auto result = all(
                            resolve<void, int>(),
                            resolve<void, int>(),
                            reject<void, int>(-1),
                            resolve<void, int>(),
                            resolve<void, int>()
                        ).get();
                        REQUIRE_FALSE(result);
                        REQUIRE(result.error() == -1);
                    }
                }

                SECTION("not void") {
                    SECTION("resolve") {
                        const auto result = all(
                            resolve<int, int>(1),
                            resolve<int, int>(2),
                            resolve<int, int>(3),
                            resolve<int, int>(4),
                            resolve<int, int>(5)
                        ).get();
                        REQUIRE(result);

                        const auto [r1, r2, r3, r4, r5] = *result;
                        REQUIRE(r1 == 1);
                        REQUIRE(r2 == 2);
                        REQUIRE(r3 == 3);
                        REQUIRE(r4 == 4);
                        REQUIRE(r5 == 5);
                    }

                    SECTION("reject") {
                        const auto result = all(
                            resolve<int, int>(1),
                            resolve<int, int>(2),
                            reject<int, int>(-1),
                            resolve<int, int>(4),
                            resolve<int, int>(5)
                        ).get();
                        REQUIRE_FALSE(result);
                        REQUIRE(result.error() == -1);
                    }
                }
            }

            SECTION("different types") {
                SECTION("resolve") {
                    const auto result = all(
                        resolve<int, int>(1),
                        resolve<void, int>(),
                        resolve<long, int>(2),
                        resolve<long, int>(3),
                        resolve<long, int>(4)
                    ).get();
                    REQUIRE(result);
                    const auto [r1, r2, r3, r4, r5] = *result;

                    REQUIRE(r1 == 1);
                    REQUIRE(r3 == 2);
                    REQUIRE(r4 == 3);
                    REQUIRE(r5 == 4);
                }

                SECTION("reject") {
                    const auto result = all(
                        resolve<int, int>(1),
                        reject<void, int>(-1),
                        reject<void, int>(-2),
                        reject<void, int>(-3),
                        resolve<int, int>(2),
                        resolve<int, int>(3)
                    ).get();
                    REQUIRE_FALSE(result);
                    const auto error = result.error();
                    REQUIRE((error == -1 || error == -2 || error == -3));
                }
            }
        }
    }

    SECTION("promise::allSettled") {
        SECTION("ranges") {
            SECTION("void") {
                const auto result = allSettled(std::array{
                    resolve<void, int>(),
                    reject<void, int>(-1),
                    resolve<void, int>(),
                    resolve<void, int>(),
                    resolve<void, int>()
                }).get();
                REQUIRE(result);

                REQUIRE(result->at(0));
                REQUIRE(result->at(1).error() == -1);
                REQUIRE(result->at(2));
                REQUIRE(result->at(3));
                REQUIRE(result->at(4));
            }

            SECTION("not void") {
                const auto result = allSettled(std::array{
                    resolve<int, int>(1),
                    reject<int, int>(-1),
                    resolve<int, int>(2),
                    resolve<int, int>(3),
                    resolve<int, int>(4)
                }).get();
                REQUIRE(result);

                REQUIRE(result->at(0).value() == 1);
                REQUIRE(result->at(1).error() == -1);
                REQUIRE(result->at(2).value() == 2);
                REQUIRE(result->at(3).value() == 3);
                REQUIRE(result->at(4).value() == 4);
            }
        }

        SECTION("variadic") {
            SECTION("same types") {
                SECTION("void") {
                    const auto result = allSettled(
                        resolve<void, int>(),
                        reject<void, int>(-1),
                        resolve<void, int>(),
                        resolve<void, int>(),
                        resolve<void, int>()
                    ).get();
                    REQUIRE(result);

                    const auto [r1, r2, r3, r4, r5] = *result;
                    REQUIRE(r1);
                    REQUIRE(r2.error() == -1);
                    REQUIRE(r3);
                    REQUIRE(r4);
                    REQUIRE(r5);
                }

                SECTION("not void") {
                    const auto result = allSettled(
                        resolve<int, int>(1),
                        reject<int, int>(-1),
                        resolve<int, int>(2),
                        resolve<int, int>(3),
                        resolve<int, int>(4)
                    ).get();
                    REQUIRE(result);

                    const auto [r1, r2, r3, r4, r5] = *result;
                    REQUIRE(r1.value() == 1);
                    REQUIRE(r2.error() == -1);
                    REQUIRE(r3.value() == 2);
                    REQUIRE(r4.value() == 3);
                    REQUIRE(r5.value() == 4);
                }
            }

            SECTION("different types") {
                const auto result = allSettled(
                    resolve<int, int>(1),
                    reject<void, int>(-1),
                    resolve<long, int>(2L),
                    resolve<long, long>(3),
                    resolve<int, int>(4),
                    resolve<void, int>()
                ).get();
                REQUIRE(result);

                const auto [r1, r2, r3, r4, r5, r6] = *result;
                REQUIRE(r1.value() == 1);
                REQUIRE(r2.error() == -1);
                REQUIRE(r3.value() == 2);
                REQUIRE(r4.value() == 3);
                REQUIRE(r5.value() == 4);
                REQUIRE(r6);
            }
        }
    }

    SECTION("promise::any") {
        SECTION("ranges") {
            SECTION("void") {
                SECTION("resolve") {
                    const auto result = any(std::array{
                        reject<void, int>(-1),
                        reject<void, int>(-2),
                        reject<void, int>(-3),
                        reject<void, int>(-4),
                        reject<void, int>(-5),
                        reject<void, int>(-6),
                        resolve<void, int>()
                    }).get();
                    REQUIRE(result);
                }

                SECTION("reject") {
                    const auto result = any(std::array{
                        reject<void, int>(-1),
                        reject<void, int>(-2),
                        reject<void, int>(-3),
                        reject<void, int>(-4),
                        reject<void, int>(-5),
                        reject<void, int>(-6),
                        reject<void, int>(-7)
                    }).get();
                    REQUIRE_FALSE(result);

                    const auto &errors = result.error();
                    REQUIRE(errors[0] == -1);
                    REQUIRE(errors[1] == -2);
                    REQUIRE(errors[2] == -3);
                    REQUIRE(errors[3] == -4);
                    REQUIRE(errors[4] == -5);
                    REQUIRE(errors[5] == -6);
                    REQUIRE(errors[6] == -7);
                }
            }

            SECTION("not void") {
                SECTION("resolve") {
                    const auto result = any(std::array{
                        reject<int, int>(-1),
                        reject<int, int>(-2),
                        reject<int, int>(-3),
                        reject<int, int>(-4),
                        reject<int, int>(-5),
                        reject<int, int>(-6),
                        resolve<int, int>(1)
                    }).get();
                    REQUIRE(result);
                    REQUIRE(*result == 1);
                }

                SECTION("reject") {
                    const auto result = any(std::array{
                        reject<int, int>(-1),
                        reject<int, int>(-2),
                        reject<int, int>(-3),
                        reject<int, int>(-4),
                        reject<int, int>(-5),
                        reject<int, int>(-6),
                        reject<int, int>(-7)
                    }).get();
                    REQUIRE_FALSE(result);

                    const auto &errors = result.error();
                    REQUIRE(errors[0] == -1);
                    REQUIRE(errors[1] == -2);
                    REQUIRE(errors[2] == -3);
                    REQUIRE(errors[3] == -4);
                    REQUIRE(errors[4] == -5);
                    REQUIRE(errors[5] == -6);
                    REQUIRE(errors[6] == -7);
                }
            }
        }

        SECTION("variadic") {
            SECTION("same types") {
                SECTION("void") {
                    SECTION("resolve") {
                        const auto result = any(
                            reject<void, int>(-1),
                            reject<void, int>(-2),
                            reject<void, int>(-3),
                            reject<void, int>(-4),
                            reject<void, int>(-5),
                            reject<void, int>(-6),
                            resolve<void, int>()
                        ).get();
                        REQUIRE(result);
                    }

                    SECTION("reject") {
                        SECTION("resolve") {
                            const auto result = any(
                                reject<void, int>(-1),
                                reject<void, int>(-2),
                                reject<void, int>(-3),
                                reject<void, int>(-4),
                                reject<void, int>(-5),
                                reject<void, int>(-6),
                                reject<void, int>(-7)
                            ).get();
                            REQUIRE_FALSE(result);

                            const auto &errors = result.error();
                            REQUIRE(errors[0] == -1);
                            REQUIRE(errors[1] == -2);
                            REQUIRE(errors[2] == -3);
                            REQUIRE(errors[3] == -4);
                            REQUIRE(errors[4] == -5);
                            REQUIRE(errors[5] == -6);
                            REQUIRE(errors[6] == -7);
                        }
                    }
                }

                SECTION("not void") {
                    SECTION("resolve") {
                        const auto result = any(
                            reject<int, int>(-1),
                            reject<int, int>(-2),
                            reject<int, int>(-3),
                            reject<int, int>(-4),
                            reject<int, int>(-5),
                            reject<int, int>(-6),
                            resolve<int, int>(1)
                        ).get();
                        REQUIRE(result);
                        REQUIRE(*result == 1);
                    }

                    SECTION("reject") {
                        SECTION("resolve") {
                            const auto result = any(
                                reject<int, int>(-1),
                                reject<int, int>(-2),
                                reject<int, int>(-3),
                                reject<int, int>(-4),
                                reject<int, int>(-5),
                                reject<int, int>(-6),
                                reject<int, int>(-7)
                            ).get();
                            REQUIRE_FALSE(result);

                            const auto &errors = result.error();
                            REQUIRE(errors[0] == -1);
                            REQUIRE(errors[1] == -2);
                            REQUIRE(errors[2] == -3);
                            REQUIRE(errors[3] == -4);
                            REQUIRE(errors[4] == -5);
                            REQUIRE(errors[5] == -6);
                            REQUIRE(errors[6] == -7);
                        }
                    }
                }
            }

#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 190000
            SECTION("different types") {
                SECTION("void") {
                    SECTION("resolve") {
                        const auto result = any(
                            reject<void, int>(-1),
                            reject<long, int>(-2),
                            reject<void, int>(-3),
                            reject<long, int>(-4),
                            reject<void, int>(-5),
                            reject<long, int>(-6),
                            resolve<void, int>()
                        ).get();
                        REQUIRE(result);
                        REQUIRE_FALSE(result->has_value());
                    }

                    SECTION("reject") {
                        const auto result = any(
                            reject<void, int>(-1),
                            reject<long, int>(-2),
                            reject<void, int>(-3),
                            reject<long, int>(-4),
                            reject<void, int>(-5),
                            reject<long, int>(-6),
                            reject<void, int>(-7)
                        ).get();
                        REQUIRE_FALSE(result);

                        const auto &errors = result.error();
                        REQUIRE(errors[0] == -1);
                        REQUIRE(errors[1] == -2);
                        REQUIRE(errors[2] == -3);
                        REQUIRE(errors[3] == -4);
                        REQUIRE(errors[4] == -5);
                        REQUIRE(errors[5] == -6);
                        REQUIRE(errors[6] == -7);
                    }
                }

                SECTION("not void") {
                    SECTION("resolve") {
                        const auto result = any(
                            reject<void, int>(-1),
                            reject<long, int>(-2),
                            reject<void, int>(-3),
                            reject<long, int>(-4),
                            reject<void, int>(-5),
                            reject<long, int>(-6),
                            resolve<int, int>(1)
                        ).get();

                        REQUIRE(result);
                        const auto &any = *result;
#if defined(_CPPRTTI) || defined(__GXX_RTTI)
                        REQUIRE(any.type() == typeid(int));
#endif
                        REQUIRE(std::any_cast<int>(any) == 1);
                    }

                    SECTION("reject") {
                        const auto result = any(
                            reject<void, int>(-1),
                            reject<long, int>(-2),
                            reject<void, int>(-3),
                            reject<long, int>(-4),
                            reject<void, int>(-5),
                            reject<long, int>(-6),
                            reject<int, int>(-7)
                        ).get();
                        REQUIRE_FALSE(result);

                        const auto &errors = result.error();
                        REQUIRE(errors[0] == -1);
                        REQUIRE(errors[1] == -2);
                        REQUIRE(errors[2] == -3);
                        REQUIRE(errors[3] == -4);
                        REQUIRE(errors[4] == -5);
                        REQUIRE(errors[5] == -6);
                        REQUIRE(errors[6] == -7);
                    }
                }
            }
#endif
        }
    }

    SECTION("promise::race") {
        SECTION("ranges") {
            SECTION("void") {
                if (const auto result = race(std::array{
                    resolve<void, int>(),
                    reject<void, int>(-1),
                    resolve<void, int>(),
                    reject<void, int>(-2),
                    resolve<void, int>(),
                    reject<void, int>(-3)
                }).get(); !result) {
                    const auto error = result.error();
                    REQUIRE((error == -1 || error == -2 || error == -3));
                }
            }

            SECTION("not void") {
                if (const auto result = race(std::array{
                    resolve<int, int>(1),
                    reject<int, int>(-1),
                    resolve<int, int>(2),
                    reject<int, int>(-2),
                    resolve<int, int>(3),
                    reject<int, int>(-3)
                }).get(); result) {
                    const auto value = *result;
                    REQUIRE((value == 1 || value == 2 || value == 3));
                }
                else {
                    const auto error = result.error();
                    REQUIRE((error == -1 || error == -2 || error == -3));
                }
            }
        }

        SECTION("variadic") {
            SECTION("same types") {
                SECTION("void") {
                    if (const auto result = race(
                        resolve<void, int>(),
                        reject<void, int>(-1),
                        resolve<void, int>(),
                        reject<void, int>(-2),
                        resolve<void, int>(),
                        reject<void, int>(-3)
                    ).get(); !result) {
                        const auto error = result.error();
                        REQUIRE((error == -1 || error == -2 || error == -3));
                    }
                }

                SECTION("not void") {
                    if (const auto result = race(
                        resolve<int, int>(1),
                        reject<int, int>(-1),
                        resolve<int, int>(2),
                        reject<int, int>(-2),
                        resolve<int, int>(3),
                        reject<int, int>(-3)
                    ).get(); result) {
                        const auto value = *result;
                        REQUIRE((value == 1 || value == 2 || value == 3));
                    }
                    else {
                        const auto error = result.error();
                        REQUIRE((error == -1 || error == -2 || error == -3));
                    }
                }
            }

#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 190000
            SECTION("different types") {
                if (const auto result = race(
                    resolve<int, int>(1),
                    reject<long, int>(-1),
                    resolve<int, int>(2),
                    reject<long, int>(-2),
                    resolve<int, int>(3),
                    reject<long, int>(-3)
                ).get(); result) {
                    const auto &any = *result;
#if defined(_CPPRTTI) || defined(__GXX_RTTI)
                    REQUIRE(any.type() == typeid(int));
#endif
                    const auto value = std::any_cast<int>(any);
                    REQUIRE((value == 1 || value == 2 || value == 3));
                }
                else {
                    const auto error = result.error();
                    REQUIRE((error == -1 || error == -2 || error == -3));
                }
            }
#endif
        }
    }
}
