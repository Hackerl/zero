#include <zero/async/coroutine.h>
#include <catch2/catch_test_macros.hpp>

zero::async::coroutine::Task<int> createTask(const zero::async::promise::Promise<int, int> *promise) {
    tl::expected<int, int> result = co_await *promise;

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return result.value() * 10;
}

zero::async::coroutine::Task<long> createTaskL(const zero::async::promise::Promise<long, int> *promise) {
    tl::expected<long, int> result = co_await *promise;

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return result.value() * 10;
}

zero::async::coroutine::Task<int> createCancellableTask(zero::async::promise::Promise<int, int> *promise) {
    tl::expected<int, int> result = co_await zero::async::coroutine::Cancellable{
            *promise,
            [=]() -> tl::expected<void, std::error_code> {
                promise->reject(-1);
                return {};
            }
    };

    if (!result)
        throw std::runtime_error(std::to_string(result.error()));

    co_return result.value() * 10;
}

zero::async::coroutine::Task<int, int> createTaskE(const zero::async::promise::Promise<int, int> *promise) {
    tl::expected<int, int> result = co_await *promise;

    if (!result)
        co_return tl::unexpected(result.error());

    co_return result.value() * 10;
}

zero::async::coroutine::Task<long, int> createTaskEL(const zero::async::promise::Promise<long, int> *promise) {
    tl::expected<long, int> result = co_await *promise;

    if (!result)
        co_return tl::unexpected(result.error());

    co_return result.value() * 10;
}

zero::async::coroutine::Task<int, int> createCancellableTaskE(zero::async::promise::Promise<int, int> *promise) {
    tl::expected<int, int> result = co_await zero::async::coroutine::Cancellable{
            *promise,
            [=]() -> tl::expected<void, std::error_code> {
                promise->reject(-1);
                return {};
            }
    };

    if (!result)
        co_return tl::unexpected(result.error());

    co_return result.value() * 10;
}

TEST_CASE("C++20 coroutines", "[coroutine]") {
    SECTION("exception") {
        SECTION("success") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int> task = createTask(&promise);
            REQUIRE(!task.done());

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(task.result().value() == 100);
        }

        SECTION("failure") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int> task = createTask(&promise);
            REQUIRE(!task.done());

            promise.reject(1024);
            REQUIRE(task.done());

            tl::expected<int, std::exception_ptr> result = task.result();
            REQUIRE(!result);

            try {
                std::rethrow_exception(result.error());
            } catch (const std::runtime_error &error) {
                REQUIRE(strcmp(error.what(), "1024") == 0);
            }
        }

        SECTION("cancel") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int, std::exception_ptr> task = createCancellableTask(&promise);
            REQUIRE(!task.done());

            task.cancel();
            REQUIRE(task.done());

            tl::expected<int, std::exception_ptr> result = task.result();
            REQUIRE(!result);

            try {
                std::rethrow_exception(result.error());
            } catch (const std::runtime_error &error) {
                REQUIRE(strcmp(error.what(), "-1") == 0);
            }
        }

        SECTION("traceback") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int, std::exception_ptr> task = createCancellableTask(&promise);
            REQUIRE(!task.done());

            auto callstack = task.traceback();
            REQUIRE(strstr(callstack[0].function_name(), "createCancellableTask"));

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(task.traceback().empty());
        }

        SECTION("coroutine::all") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<std::array<int, 2>> task = zero::async::coroutine::all(
                            createTask(&promise1),
                            createTask(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == std::array{100, 110});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<std::array<int, 2>> task = zero::async::coroutine::all(
                            createTask(&promise1),
                            createTask(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    tl::expected<std::array<int, 2>, std::exception_ptr> result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::tuple<int, long>> task = zero::async::coroutine::all(
                            createTask(&promise1),
                            createTaskL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11L);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == std::tuple{100, 110L});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::tuple<int, long>> task = zero::async::coroutine::all(
                            createTask(&promise1),
                            createTaskL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    tl::expected<std::tuple<int, long>, std::exception_ptr> result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }
            }
        }

        SECTION("coroutine::allSettled") {
            SECTION("success") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(
                        createTask(&promise1),
                        createTask(&promise2)
                );

                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.resolve(11);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(std::get<0>(result.value()).value() == 100);
                REQUIRE(std::get<1>(result.value()).value() == 110);
            }

            SECTION("failure") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(
                        createTask(&promise1),
                        createTask(&promise2)
                );

                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.reject(1024);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(std::get<0>(result.value()).value() == 100);

                try {
                    std::rethrow_exception(std::get<1>(result.value()).error());
                } catch (const std::runtime_error &error) {
                    REQUIRE(strcmp(error.what(), "1024") == 0);
                }
            }
        }

        SECTION("coroutine::any") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, std::list<std::exception_ptr>> task = zero::async::coroutine::any(
                            createTask(&promise1),
                            createTask(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, std::list<std::exception_ptr>> task = zero::async::coroutine::any(
                            createTask(&promise1),
                            createTask(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<int, std::list<std::exception_ptr>> result = task.result();
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
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, std::list<std::exception_ptr>> task = zero::async::coroutine::any(
                            createTask(&promise1),
                            createTaskL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.resolve(10L);

                    REQUIRE(task.done());

                    tl::expected<std::any, std::list<std::exception_ptr>> result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result.value().type() == typeid(long));
#endif
                    REQUIRE(std::any_cast<long>(result.value()) == 100L);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, std::list<std::exception_ptr>> task = zero::async::coroutine::any(
                            createTask(&promise1),
                            createTaskL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<std::any, std::list<std::exception_ptr>> result = task.result();
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
            }
        }

        SECTION("coroutine::race") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, std::exception_ptr> task = zero::async::coroutine::race(
                            createTask(&promise1),
                            createTask(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, std::exception_ptr> task = zero::async::coroutine::race(
                            createTask(&promise1),
                            createTask(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<int, std::exception_ptr> result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, std::exception_ptr> task = zero::async::coroutine::race(
                            createTask(&promise1),
                            createTaskL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    tl::expected<std::any, std::exception_ptr> result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result.value().type() == typeid(int));
#endif
                    REQUIRE(std::any_cast<int>(result.value()) == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, std::exception_ptr> task = zero::async::coroutine::race(
                            createTask(&promise1),
                            createTaskL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<std::any, std::exception_ptr> result = task.result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    } catch (const std::runtime_error &error) {
                        REQUIRE(strcmp(error.what(), "1024") == 0);
                    }
                }
            }
        }
    }

    SECTION("error") {
        SECTION("success") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int, int> task = createTaskE(&promise);
            REQUIRE(!task.done());

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(task.result().value() == 100);
        }

        SECTION("failure") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int, int> task = createTaskE(&promise);
            REQUIRE(!task.done());

            promise.reject(1024);
            REQUIRE(task.done());
            REQUIRE(task.result().error() == 1024);
        }

        SECTION("cancel") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int, int> task = createCancellableTaskE(&promise);
            REQUIRE(!task.done());

            task.cancel();
            REQUIRE(task.done());

            tl::expected<int, int> result = task.result();
            REQUIRE(!result);
            REQUIRE(task.result().error() == -1);
        }

        SECTION("traceback") {
            zero::async::promise::Promise<int, int> promise;
            zero::async::coroutine::Task<int, int> task = createCancellableTaskE(&promise);
            REQUIRE(!task.done());

            auto callstack = task.traceback();
            REQUIRE(strstr(callstack[0].function_name(), "createCancellableTaskE"));

            promise.resolve(10);
            REQUIRE(task.done());
            REQUIRE(task.traceback().empty());
        }

        SECTION("coroutine::all") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<std::array<int, 2>, int> task = zero::async::coroutine::all(
                            createTaskE(&promise1),
                            createTaskE(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == std::array{100, 110});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<std::array<int, 2>, int> task = zero::async::coroutine::all(
                            createTaskE(&promise1),
                            createTaskE(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    tl::expected<std::array<int, 2>, int> result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::tuple<int, long>, int> task = zero::async::coroutine::all(
                            createTaskE(&promise1),
                            createTaskEL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11L);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == std::tuple{100, 110L});
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::tuple<int, long>, int> task = zero::async::coroutine::all(
                            createTaskE(&promise1),
                            createTaskEL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    tl::expected<std::tuple<int, long>, int> result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }
            }
        }

        SECTION("coroutine::allSettled") {
            SECTION("success") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(
                        createTaskE(&promise1),
                        createTaskE(&promise2)
                );

                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.resolve(11);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(std::get<0>(result.value()).value() == 100);
                REQUIRE(std::get<1>(result.value()).value() == 110);
            }

            SECTION("failure") {
                zero::async::promise::Promise<int, int> promise1;
                zero::async::promise::Promise<int, int> promise2;

                auto task = zero::async::coroutine::allSettled(
                        createTaskE(&promise1),
                        createTaskE(&promise2)
                );

                REQUIRE(!task.done());

                promise1.resolve(10);
                promise2.reject(1024);

                REQUIRE(task.done());

                auto result = task.result();
                REQUIRE(result);

                REQUIRE(std::get<0>(result.value()).value() == 100);
                REQUIRE(std::get<1>(result.value()).error() == 1024);
            }
        }

        SECTION("coroutine::any") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, std::list<int>> task = zero::async::coroutine::any(
                            createTaskE(&promise1),
                            createTaskE(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, std::list<int>> task = zero::async::coroutine::any(
                            createTaskE(&promise1),
                            createTaskE(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<int, std::list<int>> result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error().front() == 1025);
                    REQUIRE(result.error().back() == 1024);
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, std::list<int>> task = zero::async::coroutine::any(
                            createTaskE(&promise1),
                            createTaskEL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.resolve(10L);

                    REQUIRE(task.done());

                    tl::expected<std::any, std::list<int>> result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result.value().type() == typeid(long));
#endif
                    REQUIRE(std::any_cast<long>(result.value()) == 100L);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, std::list<int>> task = zero::async::coroutine::any(
                            createTaskE(&promise1),
                            createTaskEL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<std::any, std::list<int>> result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error().front() == 1025);
                    REQUIRE(result.error().back() == 1024);
                }
            }
        }

        SECTION("coroutine::race") {
            SECTION("same types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, int> task = zero::async::coroutine::race(
                            createTaskE(&promise1),
                            createTaskE(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.resolve(11);

                    REQUIRE(task.done());
                    REQUIRE(task.result().value() == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<int, int> promise2;

                    zero::async::coroutine::Task<int, int> task = zero::async::coroutine::race(
                            createTaskE(&promise1),
                            createTaskE(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<int, int> result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }
            }

            SECTION("different types") {
                SECTION("success") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, int> task = zero::async::coroutine::race(
                            createTaskE(&promise1),
                            createTaskEL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.resolve(10);
                    promise2.reject(1024);

                    REQUIRE(task.done());

                    tl::expected<std::any, int> result = task.result();
                    REQUIRE(result);

#if _CPPRTTI || __GXX_RTTI
                    REQUIRE(result.value().type() == typeid(int));
#endif
                    REQUIRE(std::any_cast<int>(result.value()) == 100);
                }

                SECTION("failure") {
                    zero::async::promise::Promise<int, int> promise1;
                    zero::async::promise::Promise<long, int> promise2;

                    zero::async::coroutine::Task<std::any, int> task = zero::async::coroutine::race(
                            createTaskE(&promise1),
                            createTaskEL(&promise2)
                    );

                    REQUIRE(!task.done());

                    promise1.reject(1024);
                    promise2.reject(1025);

                    REQUIRE(task.done());

                    tl::expected<std::any, int> result = task.result();
                    REQUIRE(!result);
                    REQUIRE(result.error() == 1024);
                }
            }
        }
    }
}