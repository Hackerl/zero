#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <any>
#include <list>
#include <string>
#include <memory>
#include <stdexcept>
#include <tl/expected.hpp>
#include <zero/detail/type_traits.h>

namespace zero::async::promise {
    template<typename T, typename E>
    class Promise;

    template<typename T>
    struct promise_next_result {
        typedef T type;
    };

    template<typename T, typename E>
    struct promise_next_result<Promise<T, E>> {
        typedef T type;
    };

    template<typename T, typename E>
    struct promise_next_result<tl::expected<T, E>> {
        typedef T type;
    };

    template<typename T>
    using promise_next_result_t = typename promise_next_result<T>::type;

    template<typename T>
    struct promise_next_reason {
        typedef T type;
    };

    template<typename T, typename E>
    struct promise_next_reason<Promise<T, E>> {
        typedef E type;
    };

    template<typename T, typename E>
    struct promise_next_reason<tl::expected<T, E>> {
        typedef E type;
    };

    template<typename T>
    struct promise_next_reason<tl::unexpected<T>> {
        typedef T type;
    };

    template<typename T>
    using promise_next_reason_t = typename promise_next_reason<T>::type;

    template<typename T>
    inline constexpr bool is_promise_v = false;

    template<typename T, typename E>
    inline constexpr bool is_promise_v<Promise<T, E>> = true;

    enum State {
        PENDING,
        FULFILLED,
        REJECTED
    };

    template<typename T, typename E>
    struct Storage {
        State status{PENDING};
        std::unique_ptr<tl::expected<T, E>> result;
        std::list<std::pair<std::function<void()>, std::function<void()>>> triggers;
    };

    template<typename T, typename E>
    class Promise {
    public:
        using value_type = T;
        using error_type = E;

    public:
        Promise() : mStorage(std::make_shared<Storage<T, E>>()) {

        }

    public:
        template<typename ...Ts>
        void resolve(Ts &&...args) {
            if (mStorage->status != PENDING)
                return;

            mStorage->status = FULFILLED;
            mStorage->result = std::make_unique<tl::expected<T, E>>(std::forward<Ts>(args)...);

            for (const auto &trigger: std::exchange(mStorage->triggers, {}))
                trigger.first();
        }

        template<typename ...Ts>
        void reject(Ts &&...args) {
            if (mStorage->status != PENDING)
                return;

            mStorage->status = REJECTED;
            mStorage->result = std::make_unique<tl::expected<T, E>>(tl::unexpected(std::forward<Ts>(args)...));

            for (const auto &trigger: std::exchange(mStorage->triggers, {}))
                trigger.second();
        }

    public:
        template<typename F>
        auto then(F &&f) {
            using Next = detail::callable_result_t<F>;
            using NextResult = std::conditional_t<detail::is_specialization<Next, tl::unexpected>, T, promise_next_result_t<Next>>;
            using NextReason = std::conditional_t<detail::is_specialization<Next, tl::unexpected>, promise_next_reason_t<Next>, E>;

            Promise<NextResult, NextReason> promise;

            auto onFulfilledTrigger = [=, f = std::forward<F>(f), &result = mStorage->result]() mutable {
                if constexpr (std::is_void_v<Next>) {
                    if constexpr (std::is_void_v<T>) {
                        f();
                    } else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, std::as_const(result->value()));
                    } else {
                        f(std::as_const(result->value()));
                    }

                    promise.resolve();
                } else {
                    Next next = [=, &result]() {
                        if constexpr (std::is_void_v<T>) {
                            return f();
                        } else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, std::as_const(result->value()));
                        } else {
                            return f(std::as_const(result->value()));
                        }
                    }();

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (detail::is_specialization<Next, tl::expected>) {
                            static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_void_v<NextResult>) {
                                    promise.resolve();
                                } else {
                                    promise.resolve(std::move(next.value()));
                                }
                            } else {
                                promise.reject(std::move(next.error()));
                            }
                        } else if constexpr (detail::is_specialization<Next, tl::unexpected>) {
                            promise.reject(std::move(next.error()));
                        } else {
                            promise.resolve(std::move(next));
                        }
                    } else {
                        static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                        if constexpr (std::is_void_v<NextResult>) {
                            next.then([=]() mutable {
                                promise.resolve();
                            }, [=](const E &reason) mutable {
                                promise.reject(reason);
                            });
                        } else {
                            next.then([=](const NextResult &result) mutable {
                                promise.resolve(result);
                            }, [=](const E &reason) mutable {
                                promise.reject(reason);
                            });
                        }
                    }
                }
            };

            auto onRejectedTrigger = [=, &result = mStorage->result]() mutable {
                static_assert(std::is_same_v<NextReason, E>);
                promise.reject(std::as_const(result->error()));
            };

            switch (mStorage->status) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mStorage->triggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
            }

            return promise;
        }

        template<typename F>
        auto fail(F &&f) {
            using Next = detail::callable_result_t<F>;
            using NextResult = std::conditional_t<detail::is_specialization<Next, tl::unexpected>, T, promise_next_result_t<Next>>;
            using NextReason = std::conditional_t<detail::is_specialization<Next, tl::unexpected>, promise_next_reason_t<Next>, E>;

            Promise<NextResult, NextReason> promise;

            auto onFulfilledTrigger = [=, &result = mStorage->result]() mutable {
                static_assert(std::is_same_v<NextResult, T>);

                if constexpr (std::is_void_v<T>) {
                    promise.resolve();
                } else {
                    promise.resolve(std::as_const(result->value()));
                }
            };

            auto onRejectedTrigger = [=, f = std::forward<F>(f), &result = mStorage->result]() mutable {
                if constexpr (std::is_void_v<Next>) {
                    if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, std::as_const(result->error()));
                    } else {
                        f(std::as_const(result->error()));
                    }

                    promise.resolve();
                } else {
                    Next next = [=, &result]() {
                        if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, std::as_const(result->error()));
                        } else {
                            return f(std::as_const(result->error()));
                        }
                    }();

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (detail::is_specialization<Next, tl::expected>) {
                            static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_void_v<NextResult>) {
                                    promise.resolve();
                                } else {
                                    promise.resolve(std::move(next.value()));
                                }
                            } else {
                                promise.reject(std::move(next.error()));
                            }
                        } else if constexpr (detail::is_specialization<Next, tl::unexpected>) {
                            promise.reject(std::move(next.value()));
                        } else {
                            promise.resolve(std::move(next));
                        }
                    } else {
                        static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                        if constexpr (std::is_void_v<NextResult>) {
                            next->then([=]() mutable {
                                promise.resolve();
                            }, [=](const E &reason) mutable {
                                promise.reject(reason);
                            });
                        } else {
                            next->then([=](const NextResult &result) mutable {
                                promise.resolve(result);
                            }, [=](const E &reason) mutable {
                                promise.reject(reason);
                            });
                        }
                    }
                }
            };

            switch (mStorage->status) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mStorage->triggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
            }

            return promise;
        }

        template<typename F>
        Promise<T, E> finally(F &&f) {
            Promise<T, E> promise;

            auto onFulfilledTrigger = [=, &result = mStorage->result]() mutable {
                f();

                if constexpr (std::is_void_v<T>) {
                    promise.resolve();
                } else {
                    promise.resolve(std::as_const(result->value()));
                }
            };

            auto onRejectedTrigger = [=, &result = mStorage->result]() mutable {
                f();
                promise.reject(std::as_const(result->error()));
            };

            switch (mStorage->status) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mStorage->triggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
            }

            return promise;
        }

        template<typename F1, typename F2>
        auto then(F1 &&f1, F2 &&f2) {
            return then(std::forward<F1>(f1)).fail(std::forward<F2>(f2));
        }

    public:
        [[nodiscard]] State status() const {
            return mStorage->status;
        }

        template<typename = void>
        requires (!std::is_void_v<T>)
        [[nodiscard]] std::add_lvalue_reference_t<std::add_const_t<T>> value() const {
            return mStorage->result->value();
        }

        [[nodiscard]] const E &reason() const {
            return mStorage->result->error();
        }

        [[nodiscard]] const tl::expected<T, E> &result() const {
            return *mStorage->result;
        }

        tl::expected<T, E> &result() {
            return *mStorage->result;
        }

    private:
        std::shared_ptr<Storage<T, E>> mStorage;

        template<size_t...Is, typename ...Ts, typename Error>
        friend Promise<std::tuple<tl::expected<Ts, Error>...>, Error>
        allSettled(std::index_sequence<Is...>, Promise<Ts, Error> &...promises);
    };

    template<typename...Ts>
    using promises_result_t = std::conditional_t<
            detail::is_elements_same_v<detail::first_element_t<Ts...>, Ts...>,
            std::conditional_t<
                    std::is_void_v<detail::first_element_t<Ts...>>,
                    void,
                    std::array<detail::first_element_t<Ts...>, sizeof...(Ts)>
            >,
            decltype(std::tuple_cat(
                    std::declval<
                            std::conditional_t<
                                    std::is_void_v<Ts>,
                                    std::tuple<>,
                                    std::tuple<Ts>
                            >
                    >()...)
            )
    >;

    template<size_t Count, size_t Index, size_t N, size_t... Is>
    struct promises_result_index_sequence : promises_result_index_sequence<Count - 1, Index + N, Is..., Index> {

    };

    template<size_t Index, size_t N, size_t... Is>
    struct promises_result_index_sequence<0, Index, N, Is...> : std::index_sequence<N, Is...> {

    };

    template<typename...Ts>
    using promises_result_index_sequence_for = promises_result_index_sequence<
            sizeof...(Ts),
            0,
            (std::is_void_v<Ts> ? 0 : 1)...
    >;

    template<typename T, typename E, typename F>
    Promise<T, E> chain(F &&f) {
        Promise<T, E> promise;
        f(promise);
        return promise;
    }

    template<typename T, typename E>
    Promise<T, E> reject(E &&reason) {
        Promise<T, E> promise;
        promise.reject(std::forward<E>(reason));
        return promise;
    }

    template<typename T, typename E>
    requires std::is_void_v<T>
    Promise<T, E> resolve() {
        Promise<T, E> promise;
        promise.resolve();
        return promise;
    }

    template<typename T, typename E>
    requires (!std::is_void_v<T>)
    Promise<T, E> resolve(T &&result) {
        Promise<T, E> promise;
        promise.resolve(std::forward<T>(result));
        return promise;
    }

    template<size_t...Is, typename ...Ts, typename E>
    Promise<promises_result_t<Ts...>, E> all(std::index_sequence<Is...>, Promise<Ts, E> &...promises) {
        Promise<promises_result_t<Ts...>, E> promise;
        auto remain = std::make_shared<size_t>(sizeof...(promises));

        if constexpr (std::is_void_v<promises_result_t<Ts...>>) {
            ([&]() {
                promises.then([=]() mutable {
                    if (--(*remain) > 0)
                        return;

                    promise.resolve();
                }, [=](const E &reason) mutable {
                    promise.reject(reason);
                });
            }(), ...);
        } else {
            auto results = std::make_shared<promises_result_t<Ts...>>();

            ([&]() {
                if constexpr (std::is_void_v<Ts>) {
                    promises.then([=]() mutable {
                        if (--(*remain) > 0)
                            return;

                        promise.resolve(std::move(*results));
                    }, [=](const E &reason) mutable {
                        promise.reject(reason);
                    });
                } else {
                    promises.then([=](const Ts &result) mutable {
                        std::get<Is>(*results) = result;

                        if (--(*remain) > 0)
                            return;

                        promise.resolve(std::move(*results));
                    }, [=](const E &reason) mutable {
                        promise.reject(reason);
                    });
                }
            }(), ...);
        }

        return promise;
    }

    template<typename ...Ts, typename E>
    Promise<promises_result_t<Ts...>, E> all(Promise<Ts, E> &...promises) {
        return all(promises_result_index_sequence_for<Ts...>{}, promises...);
    }

    template<typename ...Ts>
    requires (is_promise_v<std::decay_t<Ts>> && ...)
    auto all(Ts &&...promises) {
        return all(promises...);
    }

    template<size_t...Is, typename ...Ts, typename E>
    Promise<std::tuple<tl::expected<Ts, E>...>, E> allSettled(std::index_sequence<Is...>, Promise<Ts, E> &...promises) {
        using T = std::tuple<tl::expected<Ts, E>...>;

        Promise<T, E> promise;

        auto results = std::make_shared<T>();
        auto remain = std::make_shared<size_t>(sizeof...(Ts));

        ([&]() {
            promises.finally([=, &result = promises.mStorage->result]() mutable {
                std::get<Is>(*results) = *result;

                if (--(*remain) > 0)
                    return;

                promise.resolve(std::move(*results));
            });
        }(), ...);

        return promise;
    }

    template<typename ...Ts, typename E>
    Promise<std::tuple<tl::expected<Ts, E>...>, E> allSettled(Promise<Ts, E> &...promises) {
        return allSettled(std::index_sequence_for<Ts...>{}, promises...);
    }

    template<typename ...Ts>
    auto allSettled(Ts &&...promises) {
        return allSettled(promises...);
    }

    template<typename ...Ts, typename E>
    auto any(Promise<Ts, E> &...promises) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        Promise<std::conditional_t<Same, T, std::any>, std::list<E>> promise;

        auto remain = std::make_shared<size_t>(sizeof...(Ts));
        auto reasons = std::make_shared<std::list<E>>();

        ([&]() {
            if constexpr (std::is_void_v<Ts>) {
                promises.then([=]() mutable {
                    if constexpr (Same)
                        promise.resolve();
                    else
                        promise.resolve(std::any{});
                }, [=](const E &reason) mutable {
                    reasons->push_front(reason);

                    if (--(*remain) > 0)
                        return;

                    promise.reject(std::move(*reasons));
                });
            } else {
                promises.then([=](const Ts &result) mutable {
                    promise.resolve(result);
                }, [=](const E &reason) mutable {
                    reasons->push_front(reason);

                    if (--(*remain) > 0)
                        return;

                    promise.reject(std::move(*reasons));
                });
            }
        }(), ...);

        return promise;
    }

    template<typename ...Ts>
    auto any(Ts &&...promises) {
        return any(promises...);
    }

    template<typename ...Ts,typename E>
    auto race(Promise<Ts, E> &...promises) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        Promise<std::conditional_t<Same, T, std::any>, E> promise;

        ([&]() {
            if constexpr (std::is_void_v<Ts>) {
                promises.then([=]() mutable {
                    if constexpr (Same)
                        promise.resolve();
                    else
                        promise.resolve(std::any{});
                }, [=](const E &reason) mutable {
                    promise.reject(reason);
                });
            } else {
                promises.then([=](const Ts &result) mutable {
                    promise.resolve(result);
                }, [=](const E &reason) mutable {
                    promise.reject(reason);
                });
            }
        }(), ...);

        return promise;
    }

    template<typename ...Ts>
    auto race(Ts &&...promises) {
        return race(promises...);
    }
}

#endif //ZERO_PROMISE_H
