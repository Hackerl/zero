#include <zero/async/coroutine.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("C++20 coroutines with exception", "[coroutine]") {
    SECTION("success") {
        zero::async::promise::Promise<int, std::exception_ptr> promise;
        auto task = zero::async::coroutine::from(promise.getFuture());
        REQUIRE(!task.done());

        promise.resolve(10);
        REQUIRE(task.done());

        const auto &result = task.future().result();
        REQUIRE(result);
        REQUIRE(*result == 10);
    }

    SECTION("failure") {
        zero::async::promise::Promise<int, std::exception_ptr> promise;
        auto task = zero::async::coroutine::from(promise.getFuture());
        REQUIRE(!task.done());

        promise.reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
        REQUIRE(task.done());

        const auto &result = task.future().result();
        REQUIRE(!result);

        try {
            std::rethrow_exception(result.error());
        }
        catch (const std::system_error &error) {
            REQUIRE(error.code() == std::errc::invalid_argument);
        };
    }

    SECTION("cancel") {
        const auto promise = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();
        auto task = from(zero::async::coroutine::Cancellable{
            promise->getFuture(),
            [=]() -> tl::expected<void, std::error_code> {
                promise->reject(
                    std::make_exception_ptr(std::system_error(make_error_code(std::errc::operation_canceled))));
                return {};
            }
        });
        REQUIRE(!task.done());
        REQUIRE(!task.cancelled());
        REQUIRE(task.cancel());
        REQUIRE(task.cancelled());
        REQUIRE(task.done());

        const auto &result = task.future().result();
        REQUIRE(!result);

        try {
            std::rethrow_exception(result.error());
        }
        catch (const std::system_error &error) {
            REQUIRE(error.code() == std::errc::operation_canceled);
        };
    }

    SECTION("check cancelled") {
        const auto promise = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();
        auto task = [](auto p) -> zero::async::coroutine::Task<void> {
            bool cancelled = co_await zero::async::coroutine::cancelled;
            REQUIRE(!cancelled);

            const auto result = co_await zero::async::coroutine::Cancellable{
                p->getFuture(),
                [=]() -> tl::expected<void, std::error_code> {
                    p->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::operation_canceled))));
                    return {};
                }
            };

            REQUIRE(!result);

            try {
                std::rethrow_exception(result.error());
            }
            catch (const std::system_error &error) {
                REQUIRE(error.code() == std::errc::operation_canceled);
            };

            cancelled = co_await zero::async::coroutine::cancelled;
            REQUIRE(cancelled);
        }(promise);
        REQUIRE(!task.done());
        REQUIRE(!task.cancelled());
        REQUIRE(task.cancel());
        REQUIRE(task.cancelled());
        REQUIRE(task.done());
    }

    SECTION("traceback") {
        zero::async::promise::Promise<int, std::exception_ptr> promise;
        auto task = zero::async::coroutine::from(promise.getFuture());
        REQUIRE(!task.done());

        const auto callstack = task.traceback();
        REQUIRE(!callstack.empty());
        REQUIRE(strstr(callstack[0].function_name(), "from"));

        promise.resolve(10);
        REQUIRE(task.done());
        REQUIRE(task.traceback().empty());

        const auto &result = task.future().result();
        REQUIRE(result);
        REQUIRE(*result == 10);
    }

    SECTION("ranges") {
        SECTION("void") {
            const auto promise1 = std::make_shared<zero::async::promise::Promise<void, std::exception_ptr>>();
            const auto promise2 = std::make_shared<zero::async::promise::Promise<void, std::exception_ptr>>();

            std::array tasks{
                from(zero::async::coroutine::Cancellable{
                    promise1->getFuture(),
                    [=]() -> tl::expected<void, std::error_code> {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::operation_canceled))));
                        return {};
                    }
                }),
                from(zero::async::coroutine::Cancellable{
                    promise2->getFuture(),
                    [=]() -> tl::expected<void, std::error_code> {
                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::operation_canceled))));
                        return {};
                    }
                })
            };

            SECTION("all") {
                auto task = all(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve();
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(task.done());
                    REQUIRE(task.future().result());
                }

                SECTION("failure") {
                    promise1->resolve();
                    REQUIRE(!task.done());

                    promise2->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    promise1->resolve();
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("allSettled") {
                auto task = allSettled(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve();
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0));
                    REQUIRE(result->at(1));
                }

                SECTION("failure") {
                    promise1->resolve();
                    REQUIRE(!task.done());

                    promise2->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0));
                    REQUIRE(!result->at(1));

                    try {
                        std::rethrow_exception(result->at(1).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    promise1->resolve();
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0));
                    REQUIRE(!result->at(1));

                    try {
                        std::rethrow_exception(result->at(1).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("any") {
                auto task = any(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(task.done());
                    REQUIRE(task.future().result());
                }

                SECTION("failure") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());

                    promise2->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().at(0));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };

                    try {
                        std::rethrow_exception(result.error().at(1));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::io_error);
                    };
                }

                SECTION("cancel") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().at(0));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };

                    try {
                        std::rethrow_exception(result.error().at(1));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("race") {
                auto task = race(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve();
                    REQUIRE(promise2->isFulfilled());
                    REQUIRE(task.done());
                    REQUIRE(task.future().result());
                }

                SECTION("failure") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(promise2->isFulfilled());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }
        }

        SECTION("not void") {
            const auto promise1 = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();
            const auto promise2 = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();

            std::array tasks{
                from(zero::async::coroutine::Cancellable{
                    promise1->getFuture(),
                    [=]() -> tl::expected<void, std::error_code> {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::operation_canceled))));
                        return {};
                    }
                }),
                from(zero::async::coroutine::Cancellable{
                    promise2->getFuture(),
                    [=]() -> tl::expected<void, std::error_code> {
                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::operation_canceled))));
                        return {};
                    }
                })
            };

            SECTION("all") {
                auto task = all(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->resolve(100);
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0) == 10);
                    REQUIRE(result->at(1) == 100);
                }

                SECTION("failure") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("allSettled") {
                auto task = allSettled(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->resolve(100);
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0));
                    REQUIRE(*result->at(0) == 10);
                    REQUIRE(result->at(1));
                    REQUIRE(*result->at(1) == 100);
                }

                SECTION("failure") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0));
                    REQUIRE(*result->at(0) == 10);
                    REQUIRE(!result->at(1));

                    try {
                        std::rethrow_exception(result->at(1).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(result->at(0));
                    REQUIRE(!result->at(1));

                    try {
                        std::rethrow_exception(result->at(1).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("any") {
                auto task = any(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());

                    promise2->resolve(100);
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(*result == 100);
                }

                SECTION("failure") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());

                    promise2->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().at(0));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };

                    try {
                        std::rethrow_exception(result.error().at(1));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::io_error);
                    };
                }

                SECTION("cancel") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().at(0));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };

                    try {
                        std::rethrow_exception(result.error().at(1));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("race") {
                auto task = race(std::move(tasks));
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve(10);
                    REQUIRE(promise2->isFulfilled());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(*result == 10);
                }

                SECTION("failure") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(promise2->isFulfilled());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }
        }
    }

    SECTION("variadic") {
        SECTION("same types") {
            SECTION("void") {
                const auto promise1 = std::make_shared<zero::async::promise::Promise<void, std::exception_ptr>>();
                const auto promise2 = std::make_shared<zero::async::promise::Promise<void, std::exception_ptr>>();

                SECTION("all") {
                    auto task = all(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->resolve();
                        REQUIRE(!task.done());

                        promise2->resolve();
                        REQUIRE(task.done());
                        REQUIRE(task.future().result());
                    }

                    SECTION("failure") {
                        promise1->resolve();
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };
                    }

                    SECTION("cancel") {
                        promise1->resolve();
                        REQUIRE(!task.done());
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }

                SECTION("allSettled") {
                    auto task = allSettled(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->resolve();
                        REQUIRE(!task.done());

                        promise2->resolve();
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(std::get<0>(*result));
                        REQUIRE(std::get<1>(*result));
                    }

                    SECTION("failure") {
                        promise1->resolve();
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(std::get<0>(*result));
                        REQUIRE(!std::get<1>(*result));

                        try {
                            std::rethrow_exception(std::get<1>(*result).error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };
                    }

                    SECTION("cancel") {
                        promise1->resolve();
                        REQUIRE(!task.done());
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(std::get<0>(*result));
                        REQUIRE(!std::get<1>(*result));

                        try {
                            std::rethrow_exception(std::get<1>(*result).error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }

                SECTION("any") {
                    auto task = any(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());

                        promise2->resolve();
                        REQUIRE(task.done());
                        REQUIRE(task.future().result());
                    }

                    SECTION("failure") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error().at(0));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };

                        try {
                            std::rethrow_exception(result.error().at(1));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::io_error);
                        };
                    }

                    SECTION("cancel") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error().at(0));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };

                        try {
                            std::rethrow_exception(result.error().at(1));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }

                SECTION("race") {
                    auto task = race(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->resolve();
                        REQUIRE(promise2->isFulfilled());
                        REQUIRE(task.done());
                        REQUIRE(task.future().result());
                    }

                    SECTION("failure") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(promise2->isFulfilled());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };
                    }

                    SECTION("cancel") {
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }
            }

            SECTION("not void") {
                const auto promise1 = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();
                const auto promise2 = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();

                SECTION("all") {
                    auto task = all(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->resolve(10);
                        REQUIRE(!task.done());

                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(result->at(0) == 10);
                        REQUIRE(result->at(1) == 100);
                    }

                    SECTION("failure") {
                        promise1->resolve(10);
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };
                    }

                    SECTION("cancel") {
                        promise1->resolve(10);
                        REQUIRE(!task.done());
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }

                SECTION("allSettled") {
                    auto task = allSettled(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->resolve(10);
                        REQUIRE(!task.done());

                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(std::get<0>(*result));
                        REQUIRE(*std::get<0>(*result) == 10);
                        REQUIRE(std::get<1>(*result));
                        REQUIRE(*std::get<1>(*result) == 100);
                    }

                    SECTION("failure") {
                        promise1->resolve(10);
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(std::get<0>(*result));
                        REQUIRE(*std::get<0>(*result) == 10);
                        REQUIRE(!std::get<1>(*result));

                        try {
                            std::rethrow_exception(std::get<1>(*result).error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };
                    }

                    SECTION("cancel") {
                        promise1->resolve(10);
                        REQUIRE(!task.done());
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(std::get<0>(*result));
                        REQUIRE(!std::get<1>(*result));

                        try {
                            std::rethrow_exception(std::get<1>(*result).error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }

                SECTION("any") {
                    auto task = any(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());

                        promise2->resolve(100);
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(*result == 100);
                    }

                    SECTION("failure") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error().at(0));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };

                        try {
                            std::rethrow_exception(result.error().at(1));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::io_error);
                        };
                    }

                    SECTION("cancel") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error().at(0));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };

                        try {
                            std::rethrow_exception(result.error().at(1));
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }

                SECTION("race") {
                    auto task = race(
                        from(zero::async::coroutine::Cancellable{
                            promise1->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise1->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        }),
                        from(zero::async::coroutine::Cancellable{
                            promise2->getFuture(),
                            [=]() -> tl::expected<void, std::error_code> {
                                promise2->reject(
                                    std::make_exception_ptr(
                                        std::system_error(make_error_code(std::errc::operation_canceled))));
                                return {};
                            }
                        })
                    );
                    REQUIRE(!task.done());

                    SECTION("success") {
                        promise1->resolve(10);
                        REQUIRE(promise2->isFulfilled());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(*result == 10);
                    }

                    SECTION("failure") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(promise2->isFulfilled());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::invalid_argument);
                        };
                    }

                    SECTION("cancel") {
                        REQUIRE(task.cancel());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(!result);

                        try {
                            std::rethrow_exception(result.error());
                        }
                        catch (const std::system_error &error) {
                            REQUIRE(error.code() == std::errc::operation_canceled);
                        };
                    }
                }
            }
        }

        SECTION("different types") {
            const auto promise1 = std::make_shared<zero::async::promise::Promise<int, std::exception_ptr>>();
            const auto promise2 = std::make_shared<zero::async::promise::Promise<void, std::exception_ptr>>();
            const auto promise3 = std::make_shared<zero::async::promise::Promise<long, std::exception_ptr>>();

            SECTION("all") {
                auto task = all(
                    from(zero::async::coroutine::Cancellable{
                        promise1->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise1->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise2->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise2->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise3->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise3->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    })
                );
                REQUIRE(!task.done());

                SECTION("success") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(!task.done());

                    promise3->resolve(1000);
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result) == 10);
                    REQUIRE(std::get<1>(*result) == 1000);
                }

                SECTION("failure") {
                    promise1->resolve(100);
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(!task.done());

                    promise3->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("allSettled") {
                auto task = allSettled(
                    from(zero::async::coroutine::Cancellable{
                        promise1->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise1->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise2->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise2->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise3->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise3->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    })
                );
                REQUIRE(!task.done());

                SECTION("success") {
                    REQUIRE(!task.done());

                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(!task.done());

                    promise3->resolve(1000);
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result));
                    REQUIRE(*std::get<0>(*result) == 10);
                    REQUIRE(std::get<1>(*result));
                    REQUIRE(std::get<2>(*result));
                    REQUIRE(*std::get<2>(*result) == 1000);
                }

                SECTION("failure") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());

                    promise2->resolve();
                    REQUIRE(!task.done());

                    promise3->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result));
                    REQUIRE(*std::get<0>(*result) == 10);
                    REQUIRE(std::get<1>(*result));
                    REQUIRE(!std::get<2>(*result));

                    try {
                        std::rethrow_exception(std::get<2>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    promise1->resolve(10);
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(result);
                    REQUIRE(std::get<0>(*result));
                    REQUIRE(*std::get<0>(*result) == 10);
                    REQUIRE(!std::get<1>(*result));

                    try {
                        std::rethrow_exception(std::get<1>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                    REQUIRE(!std::get<2>(*result));

                    try {
                        std::rethrow_exception(std::get<2>(*result).error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("any") {
                auto task = any(
                    from(zero::async::coroutine::Cancellable{
                        promise1->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise1->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise2->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise2->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise3->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise3->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    })
                );
                REQUIRE(!task.done());

                SECTION("success") {
                    SECTION("no value") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());

                        promise2->resolve();
                        REQUIRE(promise1->isFulfilled());
                        REQUIRE(promise3->isFulfilled());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(!result->has_value());
                    }

                    SECTION("has value") {
                        promise1->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                        REQUIRE(!task.done());

                        promise2->reject(
                            std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                        REQUIRE(!task.done());

                        promise3->resolve(1000);
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(result->has_value());

#if _CPPRTTI || __GXX_RTTI
                        REQUIRE(result->type() == typeid(long));
#endif
                        REQUIRE(std::any_cast<long>(*result) == 1000);
                    }
                }

                SECTION("failure") {
                    promise1->reject(std::make_exception_ptr(std::system_error(make_error_code(std::errc::io_error))));
                    REQUIRE(!task.done());

                    promise2->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());

                    promise3->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::bad_message))));
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().at(0));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::io_error);
                    };

                    try {
                        std::rethrow_exception(result.error().at(1));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };

                    try {
                        std::rethrow_exception(result.error().at(2));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::bad_message);
                    };
                }

                SECTION("cancel") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(!task.done());
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error().at(0));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };

                    try {
                        std::rethrow_exception(result.error().at(1));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };

                    try {
                        std::rethrow_exception(result.error().at(2));
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }

            SECTION("race") {
                auto task = race(
                    from(zero::async::coroutine::Cancellable{
                        promise1->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise1->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise2->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise2->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    }),
                    from(zero::async::coroutine::Cancellable{
                        promise3->getFuture(),
                        [=]() -> tl::expected<void, std::error_code> {
                            promise3->reject(
                                std::make_exception_ptr(
                                    std::system_error(make_error_code(std::errc::operation_canceled))));
                            return {};
                        }
                    })
                );
                REQUIRE(!task.done());

                SECTION("success") {
                    SECTION("no value") {
                        promise2->resolve();
                        REQUIRE(promise1->isFulfilled());
                        REQUIRE(promise3->isFulfilled());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(!result->has_value());
                    }

                    SECTION("has value") {
                        promise1->resolve(10);
                        REQUIRE(promise2->isFulfilled());
                        REQUIRE(promise3->isFulfilled());
                        REQUIRE(task.done());

                        const auto &result = task.future().result();
                        REQUIRE(result);
                        REQUIRE(result->has_value());

#if _CPPRTTI || __GXX_RTTI
                        REQUIRE(result->type() == typeid(int));
#endif
                        REQUIRE(std::any_cast<int>(*result) == 10);
                    }
                }

                SECTION("failure") {
                    promise1->reject(
                        std::make_exception_ptr(std::system_error(make_error_code(std::errc::invalid_argument))));
                    REQUIRE(promise2->isFulfilled());
                    REQUIRE(promise3->isFulfilled());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::invalid_argument);
                    };
                }

                SECTION("cancel") {
                    REQUIRE(task.cancel());
                    REQUIRE(task.done());

                    const auto &result = task.future().result();
                    REQUIRE(!result);

                    try {
                        std::rethrow_exception(result.error());
                    }
                    catch (const std::system_error &error) {
                        REQUIRE(error.code() == std::errc::operation_canceled);
                    };
                }
            }
        }
    }
}
