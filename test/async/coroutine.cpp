#include <zero/async/coroutine.h>
#include <catch2/catch_test_macros.hpp>

zero::async::coroutine::Task<int> createTask(zero::async::promise::Promise<int, int> promise) {
    auto result = co_await promise;

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return *result * 10;
}

zero::async::coroutine::Task<long> createTaskL(zero::async::promise::Promise<long, int> promise) {
    auto result = co_await promise;

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return *result * 10;
}

zero::async::coroutine::Task<int> createCancellableTask(zero::async::promise::Promise<int, int> promise) {
    auto result = co_await zero::async::coroutine::Cancellable{
            promise,
            [=]() mutable -> tl::expected<void, std::error_code> {
                promise.reject(-1);
                return {};
            }
    };

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return *result * 10;
}

zero::async::coroutine::Task<long> createCancellableTaskL(zero::async::promise::Promise<long, int> promise) {
    auto result = co_await zero::async::coroutine::Cancellable{
            promise,
            [=]() mutable -> tl::expected<void, std::error_code> {
                promise.reject(-1);
                return {};
            }
    };

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return *result * 10;
}

zero::async::coroutine::Task<int, int> createTaskE(zero::async::promise::Promise<int, int> promise) {
    auto result = co_await promise;

    if (!result)
        co_return tl::unexpected(result.error());

    co_return *result * 10;
}

zero::async::coroutine::Task<long, int> createTaskEL(zero::async::promise::Promise<long, int> promise) {
    auto result = co_await promise;

    if (!result)
        co_return tl::unexpected(result.error());

    co_return *result * 10;
}

zero::async::coroutine::Task<int, int> createCancellableTaskE(zero::async::promise::Promise<int, int> promise) {
    auto result = co_await zero::async::coroutine::Cancellable{
            promise,
            [=]() mutable -> tl::expected<void, std::error_code> {
                promise.reject(-1);
                return {};
            }
    };

    if (!result)
        co_return tl::unexpected(result.error());

    co_return *result * 10;
}

zero::async::coroutine::Task<long, int> createCancellableTaskEL(zero::async::promise::Promise<long, int> promise) {
    auto result = co_await zero::async::coroutine::Cancellable{
            promise,
            [=]() mutable -> tl::expected<void, std::error_code> {
                promise.reject(-1);
                return {};
            }
    };

    if (!result)
        co_return tl::unexpected(result.error());

    co_return *result * 10;
}

TEST_CASE("C++20 coroutines", "[coroutine]") {
    SECTION("exception") {
        SECTION("success") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createTask(promise);
            REQUIRE(!task.done());

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(*task.result() == 100);
        }

        SECTION("failure") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createTask(promise);
            REQUIRE(!task.done());

            promise.reject(1024);
            REQUIRE(task.done());

            auto result = task.result();
            REQUIRE(!result);

            try {
                std::rethrow_exception(result.error());
            } catch (const std::runtime_error &error) {
                REQUIRE(strcmp(error.what(), "1024") == 0);
            }
        }

        SECTION("cancel") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createCancellableTask(promise);
            REQUIRE(!task.done());

            task.cancel();
            REQUIRE(task.done());

            auto result = task.result();
            REQUIRE(!result);

            try {
                std::rethrow_exception(result.error());
            } catch (const std::runtime_error &error) {
                REQUIRE(strcmp(error.what(), "-1") == 0);
            }
        }

        SECTION("traceback") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createCancellableTask(promise);
            REQUIRE(!task.done());

            auto callstack = task.traceback();
            REQUIRE(strstr(callstack[0].function_name(), "createCancellableTask"));

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(task.traceback().empty());
        }

        SECTION("from promise") {
            zero::async::promise::Promise<int, std::exception_ptr> promise;
            auto task = zero::async::coroutine::from(promise);
            REQUIRE(!task.done());

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(*task.result() == 10);
        }

        SECTION("from cancellable") {
            zero::async::promise::Promise<int, std::exception_ptr> promise;
            auto task = zero::async::coroutine::from(zero::async::coroutine::Cancellable{
                promise,
                [=]() mutable -> tl::expected<void, std::error_code> {
                    promise.reject(nullptr);
                    return {};
                }
            });
            REQUIRE(!task.done());
            task.cancel();

            REQUIRE(task.done());
            REQUIRE(task.result().error() == nullptr);
        }

        SECTION("coroutine::all") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::all(createTask(promise1), createTask(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == std::array{100, 110});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::all(createTask(promise1), createTask(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::all(
                            createCancellableTask(promise1),
                            createCancellableTask(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "-1") == 0);
                    }
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::all(createTask(promise1), createTaskL(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11L);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == std::tuple{100, 110L});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::all(createTask(promise1), createTaskL(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::all(
                            createCancellableTask(promise1),
                            createCancellableTaskL(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "-1") == 0);
                    }
                }
            }
        }

        SECTION("coroutine::allSettled") {
            SECTION("success") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(createTask(promise1), createTask(promise2));
                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.resolve(11);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(*std::get<0>(*result) == 100);
                REQUIRE(*std::get<1>(*result) == 110);
            }

            SECTION("failure") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(createTask(promise1), createTask(promise2));
                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.reject(1024);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(*std::get<0>(*result) == 100);

                try {
                    std::rethrow_exception(std::get<1>(*result).error());
                } catch (const std::runtime_error &error) {
                    REQUIRE(strcmp(error.what(), "1024") == 0);
                }
            }

            SECTION("cancel") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(
                        createCancellableTask(promise1),
                        createCancellableTask(promise2)
                );

                REQUIRE(!task.done());

                task.cancel();
                REQUIRE(task.done());
                REQUIRE(promise1.result().error() == -1);
                REQUIRE(promise2.result().error() == -1);

                auto result = task.result();
                REQUIRE(result);

                try {
                    std::rethrow_exception(std::get<0>(*result).error());
                } catch (const std::runtime_error &error) {
                    REQUIRE(strcmp(error.what(), "-1") == 0);
                }

                try {
                    std::rethrow_exception(std::get<1>(*result).error());
                } catch (const std::runtime_error &error) {
                    REQUIRE(strcmp(error.what(), "-1") == 0);
                }
            }
        }

        SECTION("coroutine::any") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::any(createTask(promise1), createTask(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::any(createTask(promise1), createTask(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().front());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1025") == 0);
                    }

                    try {
                        std::rethrow_exception(result.error().back());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::any(
                            createCancellableTask(promise1),
                            createCancellableTask(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().front());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "-1") == 0);
                    }
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::any(createTask(promise1), createTaskL(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.resolve(10L);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(long));
#endif
                    REQUIRE(std::any_cast<long>(*result) == 100L);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::any(createTask(promise1), createTaskL(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().front());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1025") == 0);
                    }

                    try {
                        std::rethrow_exception(result.error().back());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::any(
                            createCancellableTask(promise1),
                            createCancellableTaskL(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().front());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "-1") == 0);
                    }
                }
            }
        }

        SECTION("coroutine::race") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::race(createTask(promise1), createTask(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::race(createTask(promise1), createTask(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::race(
                            createCancellableTask(promise1),
                            createCancellableTask(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "-1") == 0);
                    }
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::race(createTask(promise1), createTaskL(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(int));
#endif
                    REQUIRE(std::any_cast<int>(*result) == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::race(createTask(promise1), createTaskL(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::race(
                            createCancellableTask(promise1),
                            createCancellableTaskL(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "-1") == 0);
                    }
                }
            }
        }
    }

    SECTION("error") {
        SECTION("success") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createTaskE(promise);
            REQUIRE(!task.done());

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(*task.result() == 100);
        }

        SECTION("failure") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createTaskE(promise);
            REQUIRE(!task.done());

            promise.reject(1024);
            REQUIRE(task.done());
            REQUIRE(task.result().error() == 1024);
        }

        SECTION("cancel") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createCancellableTaskE(promise);
            REQUIRE(!task.done());

            task.cancel();
            REQUIRE(task.done());

            auto result = task.result();
            REQUIRE(!result);
            REQUIRE(task.result().error() == -1);
        }

        SECTION("traceback") {
            zero::async::promise::Promise<int, int> promise;
            auto task = createCancellableTaskE(promise);
            REQUIRE(!task.done());

            auto callstack = task.traceback();
            REQUIRE(strstr(callstack[0].function_name(), "createCancellableTaskE"));

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(task.traceback().empty());
        }

        SECTION("from promise") {
            zero::async::promise::Promise<int, int> promise;
            auto task = zero::async::coroutine::from(promise);
            REQUIRE(!task.done());

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(*task.result() == 10);
        }

        SECTION("from cancellable") {
            zero::async::promise::Promise<int, int> promise;
            auto task = zero::async::coroutine::from(zero::async::coroutine::Cancellable{
                    promise,
                    [=]() mutable -> tl::expected<void, std::error_code> {
                        promise.reject(1024);
                        return {};
                    }
            });
            REQUIRE(!task.done());
            task.cancel();

            REQUIRE(task.done());
            REQUIRE(task.result().error() == 1024);
        }

        SECTION("coroutine::all") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::all(createTaskE(promise1), createTaskE(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == std::array{100, 110});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::all(createTaskE(promise1), createTaskE(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::all(
                            createCancellableTaskE(promise1),
                            createCancellableTaskE(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);
                    REQUIRE(task.result().error() == -1);
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::all(createTaskE(promise1), createTaskEL(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11L);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == std::tuple{100, 110L});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::all(createTaskE(promise1), createTaskEL(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::all(
                            createCancellableTaskE(promise1),
                            createCancellableTaskEL(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);
                    REQUIRE(task.result().error() == -1);
                }
            }
        }

        SECTION("coroutine::allSettled") {
            SECTION("success") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(createTaskE(promise1), createTaskE(promise2));
                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.resolve(11);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(*std::get<0>(*result) == 100);
                REQUIRE(*std::get<1>(*result) == 110);
            }

            SECTION("failure") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(createTaskE(promise1), createTaskE(promise2));
                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.reject(1024);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(*std::get<0>(*result) == 100);
                REQUIRE(std::get<1>(*result).error() == 1024);
            }

            SECTION("cancel") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(
                        createCancellableTaskE(promise1),
                        createCancellableTaskE(promise2)
                );

                REQUIRE(!task.done());

                task.cancel();
                REQUIRE(task.done());
                REQUIRE(promise1.result().error() == -1);
                REQUIRE(promise2.result().error() == -1);

                auto result = task.result();
                REQUIRE(result);
                REQUIRE(std::get<0>(*result).error() == -1);
                REQUIRE(std::get<1>(*result).error() == -1);
            }
        }

        SECTION("coroutine::any") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::any(createTaskE(promise1), createTaskE(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::any(createTaskE(promise1), createTaskE(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error().front() == 1025);
                    REQUIRE(result.error().back() == 1024);
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::any(
                            createCancellableTaskE(promise1),
                            createCancellableTaskE(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error().front() == -1);
                    REQUIRE(result.error().back() == -1);
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::any(createTaskE(promise1), createTaskEL(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.resolve(10L);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(long));
#endif
                    REQUIRE(std::any_cast<long>(*result) == 100L);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::any(createTaskE(promise1), createTaskEL(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error().front() == 1025);
                    REQUIRE(result.error().back() == 1024);
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::any(
                            createCancellableTaskE(promise1),
                            createCancellableTaskEL(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error().front() == -1);
                    REQUIRE(result.error().back() == -1);
                }
            }
        }

        SECTION("coroutine::race") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::race(createTaskE(promise1), createTaskE(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::race(createTaskE(promise1), createTaskE(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    auto task = zero::async::coroutine::race(
                            createCancellableTaskE(promise1),
                            createCancellableTaskE(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == -1);
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::race(createTaskE(promise1), createTaskEL(promise2));
                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result->type() == typeid(int));
#endif
                    REQUIRE(std::any_cast<int>(*result) == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::race(createTaskE(promise1), createTaskEL(promise2));
                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }

                SECTION("cancel") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    auto task = zero::async::coroutine::race(
                            createCancellableTaskE(promise1),
                            createCancellableTaskEL(promise2)
                    );

                    REQUIRE(!task.done());

                    task.cancel();
                    REQUIRE(task.done());
                    REQUIRE(promise1.result().error() == -1);
                    REQUIRE(promise2.result().error() == -1);

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == -1);
                }
            }
        }
    }

    SECTION("monadic operations") {
        SECTION("and then") {
            SECTION("normal") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise;
                    auto task = createTaskE(promise).andThen([](int value) -> tl::expected<int, int> {
                        return value * 10;
                    });
                    REQUIRE(!task.done());

                    promise.resolve(10);
                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 1000);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise;
                    auto task = createTaskE(promise).andThen([](int value) -> tl::expected<int, int> {
                        return value * 10;
                    });
                    REQUIRE(!task.done());

                    promise.reject(1024);
                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }

                SECTION("void") {
                    zero::async::promise::Promise<int, int> promise;
                    auto task = createTaskE(promise).andThen([](int value) -> tl::expected<void, int> {
                        return {};
                    }).andThen([]() -> tl::expected<int, int> {
                        return 1000;
                    });
                    REQUIRE(!task.done());

                    promise.resolve(10);
                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 1000);
                }
            }

            SECTION("coroutine") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise;
                    auto task = createTaskE(promise).andThen([](int value) -> zero::async::coroutine::Task<int, int> {
                        co_return value * 10;
                    });
                    REQUIRE(!task.done());

                    promise.resolve(10);
                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 1000);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise;
                    auto task = createTaskE(promise).andThen([](int value) -> zero::async::coroutine::Task<int, int> {
                        co_return value * 10;
                    });
                    REQUIRE(!task.done());

                    promise.reject(1024);
                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }

                SECTION("void") {
                    zero::async::promise::Promise<int, int> promise;
                    auto task = createTaskE(promise).andThen([](int value) -> zero::async::coroutine::Task<void, int> {
                        co_return tl::expected<void, int>{};
                    }).andThen([]() -> zero::async::coroutine::Task<int, int> {
                        co_return 1000;
                    });
                    REQUIRE(!task.done());

                    promise.resolve(10);
                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 1000);
                }
            }
        }

        SECTION("transform") {
            SECTION("success") {
                zero::async::promise::Promise<int, int> promise;
                auto task = createTaskE(promise).transform([](int value) {
                    return value * 10;
                });
                REQUIRE(!task.done());

                promise.resolve(10);
                REQUIRE(task.done());
                REQUIRE(*task.result() == 1000);
            }

            SECTION("failure") {
                zero::async::promise::Promise<int, int> promise;
                auto task = createTaskE(promise).transform([](int value) {
                    return value * 10;
                });
                REQUIRE(!task.done());

                promise.reject(1024);
                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(!result);
                REQUIRE(result.error() == 1024);
            }

            SECTION("void") {
                zero::async::promise::Promise<int, int> promise;
                auto task = createTaskE(promise).transform([](int value) {

                }).transform([]() {
                    return 1000;
                });
                REQUIRE(!task.done());

                promise.resolve(10);
                REQUIRE(task.done());
                REQUIRE(*task.result() == 1000);
            }
        }

        SECTION("or else") {
            SECTION("normal") {
                SECTION("success") {
                    zero::async::promise::Promise<long, int> promise;
                    auto task = createTaskEL(promise).orElse([](int value) -> tl::expected<long, int> {
                        return value * 10;
                    });
                    REQUIRE(!task.done());

                    promise.reject(10);
                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<long, int> promise;
                    auto task = createTaskEL(promise).orElse([](int value) -> tl::expected<long, int> {
                        return tl::unexpected(1024);
                    });
                    REQUIRE(!task.done());

                    promise.reject(10);
                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }
            }

            SECTION("coroutine") {
                SECTION("success") {
                    zero::async::promise::Promise<long, int> promise;
                    auto task = createTaskEL(promise).orElse([](int value) -> zero::async::coroutine::Task<long, int> {
                        co_return value * 10;
                    });
                    REQUIRE(!task.done());

                    promise.reject(10);
                    REQUIRE(task.done());
                    REQUIRE(*task.result() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<long, int> promise;
                    auto task = createTaskEL(promise).orElse([](int value) -> zero::async::coroutine::Task<long, int> {
                        co_return tl::unexpected(1024);
                    });
                    REQUIRE(!task.done());

                    promise.reject(10);
                    REQUIRE(task.done());

                    auto result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }
            }
        }

        SECTION("transform error") {
            SECTION("success") {
                zero::async::promise::Promise<long, int> promise;
                auto task = createTaskEL(promise).transformError([](int value) {
                    return value * 10;
                });
                REQUIRE(!task.done());

                promise.resolve(10);
                REQUIRE(task.done());
                REQUIRE(*task.result() == 100);
            }

            SECTION("failure") {
                zero::async::promise::Promise<long, int> promise;
                auto task = createTaskEL(promise).transformError([](int value) {
                    return value * 10;
                });
                REQUIRE(!task.done());

                promise.reject(10);
                REQUIRE(task.done());
                REQUIRE(task.result().error() == 100);
            }
        }
    }
}