#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <any>
#include <list>
#include <string>
#include <memory>
#include <stdexcept>
#include <nonstd/expected.hpp>
#include <zero/detail/type_traits.h>

namespace zero::async::promise {
    template<typename T, typename E>
    class Promise;

    template<typename T>
    struct promise_result {
        typedef T type;
    };

    template<typename T, typename E>
    struct promise_result<Promise<T, E>> {
        typedef T type;
    };

    template<typename T, typename E>
    struct promise_result<nonstd::expected<T, E>> {
        typedef T type;
    };

    template<typename T>
    using promise_result_t = typename promise_result<T>::type;

    template<typename T>
    struct promise_reason {
        typedef T type;
    };

    template<typename T, typename E>
    struct promise_reason<Promise<T, E>> {
        typedef E type;
    };

    template<typename T, typename E>
    struct promise_reason<nonstd::expected<T, E>> {
        typedef E type;
    };

    template<typename T>
    struct promise_reason<nonstd::unexpected_type<T>> {
        typedef T type;
    };

    template<typename T>
    using promise_reason_t = typename promise_reason<T>::type;

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
        nonstd::expected<T, E> result;
        std::list<std::pair<std::function<void()>, std::function<void()>>> triggers;
    };

    template<typename T, typename E>
    class Promise {
    public:
        Promise() : mStorage(std::make_shared<Storage<T, E>>()) {

        }

    public:
        template<typename ...Ts>
        void resolve(Ts &&...args) {
            if (mStorage->status != PENDING)
                return;

            mStorage->status = FULFILLED;
            mStorage->result = nonstd::expected<T, E>(std::forward<Ts>(args)...);

            for (const auto &trigger: mStorage->triggers) {
                trigger.first();
            }

            mStorage->triggers.clear();
        }

        template<typename ...Ts>
        void reject(Ts &&...args) {
            if (mStorage->status != PENDING)
                return;

            mStorage->status = REJECTED;
            mStorage->result = nonstd::make_unexpected(std::forward<Ts>(args)...);

            for (const auto &trigger: mStorage->triggers) {
                trigger.second();
            }

            mStorage->triggers.clear();
        }

    public:
        template<typename F>
        auto then(F &&f) {
            using Next = detail::callable_result_t<F>;
            using NextResult = std::conditional_t<detail::is_specialization<Next, nonstd::unexpected_type>, T, promise_result_t<Next>>;
            using NextReason = std::conditional_t<detail::is_specialization<Next, nonstd::unexpected_type>, promise_reason_t<Next>, E>;

            Promise<NextResult, NextReason> promise;

            auto onFulfilledTrigger = [=, f = std::forward<F>(f), result = &mStorage->result]() mutable {
                if constexpr (std::is_same_v<Next, void>) {
                    if constexpr (std::is_same_v<T, void>) {
                        f();
                    } else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, result->value());
                    } else {
                        f(result->value());
                    }

                    promise.resolve();
                } else {
                    Next next = [=]() {
                        if constexpr (std::is_same_v<T, void>) {
                            return f();
                        } else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, result->value());
                        } else {
                            return f(result->value());
                        }
                    }();

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (detail::is_specialization<Next, nonstd::expected>) {
                            static_assert(std::is_same_v<promise_reason_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_same_v<NextResult, void>) {
                                    promise.resolve();
                                } else {
                                    promise.resolve(std::move(next.value()));
                                }
                            } else {
                                promise.reject(std::move(next.error()));
                            }
                        } else if constexpr (detail::is_specialization<Next, nonstd::unexpected_type>) {
                            promise.reject(std::move(next.error()));
                        } else {
                            promise.resolve(std::move(next));
                        }
                    } else {
                        static_assert(std::is_same_v<promise_reason_t<Next>, E>);

                        if constexpr (std::is_same_v<NextResult, void>) {
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

            auto onRejectedTrigger = [=, result = &mStorage->result]() mutable {
                static_assert(std::is_same_v<NextReason, E>);
                promise.reject(result->error());
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
            using NextResult = std::conditional_t<detail::is_specialization<Next, nonstd::unexpected_type>, T, promise_result_t<Next>>;
            using NextReason = std::conditional_t<detail::is_specialization<Next, nonstd::unexpected_type>, promise_reason_t<Next>, E>;

            Promise<NextResult, NextReason> promise;

            auto onFulfilledTrigger = [=, result = &mStorage->result]() mutable {
                static_assert(std::is_same_v<NextResult, T>);

                if constexpr (std::is_same_v<T, void>) {
                    promise.resolve();
                } else {
                    promise.resolve(result->value());
                }
            };

            auto onRejectedTrigger = [=, f = std::forward<F>(f), result = &mStorage->result]() mutable {
                if constexpr (std::is_same_v<Next, void>) {
                    if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, result->error());
                    } else {
                        f(result->error());
                    }

                    promise.resolve();
                } else {
                    Next next = [=]() {
                        if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, result->result.error());
                        } else {
                            return f(result->error());
                        }
                    }();

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (detail::is_specialization<Next, nonstd::expected>) {
                            static_assert(std::is_same_v<promise_reason_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_same_v<NextResult, void>) {
                                    promise.resolve();
                                } else {
                                    promise.resolve(std::move(next.value()));
                                }
                            } else {
                                promise.reject(std::move(next.error()));
                            }
                        } else if constexpr (detail::is_specialization<Next, nonstd::unexpected_type>) {
                            promise.reject(std::move(next.value()));
                        } else {
                            promise.resolve(std::move(next));
                        }
                    } else {
                        static_assert(std::is_same_v<promise_reason_t<Next>, E>);

                        if constexpr (std::is_same_v<NextResult, void>) {
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

            auto onFulfilledTrigger = [=, result = &mStorage->result]() mutable {
                f();

                if constexpr (std::is_same_v<T, void>) {
                    promise.resolve();
                } else {
                    promise.resolve(result->value());
                }
            };

            auto onRejectedTrigger = [=, result = &mStorage->result]() mutable {
                f();
                promise.reject(result->error());
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

        [[nodiscard]] T value() const {
            return mStorage->result.value();
        }

        [[nodiscard]] E reason() const {
            return mStorage->result.error();
        }

    private:
        std::shared_ptr<Storage<T, E>> mStorage;
    };

    template<typename...Ts>
    using promises_result_t = std::conditional_t<
            detail::is_elements_same_v<detail::first_element_t<Ts...>, Ts...>,
            std::conditional_t<
                    std::is_same_v<detail::first_element_t<Ts...>, void>,
                    void,
                    std::array<detail::first_element_t<Ts...>, sizeof...(Ts)>
            >,
            decltype(std::tuple_cat(
                    std::declval<
                            std::conditional_t<
                                    std::is_same_v<Ts, void>,
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
            (std::is_same_v<Ts, void> ? 0 : 1)...
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
    requires std::is_same_v<T, void>
    Promise<T, E> resolve() {
        Promise<T, E> promise;
        promise.resolve();
        return promise;
    }

    template<typename T, typename E>
    requires (!std::is_same_v<T, void>)
    Promise<T, E> resolve(T &&result) {
        Promise<T, E> promise;
        promise.resolve(std::forward<T>(result));
        return promise;
    }

    template<typename ...Ts, typename E>
    Promise<promises_result_t<Ts...>, E> all(Promise<Ts, E> &...promises) {
        Promise<promises_result_t<Ts...>, E> promise;
        std::shared_ptr<size_t> remain = std::make_shared<size_t>(sizeof...(promises));

#ifdef _MSC_VER
        [&, &...promises = promises]<size_t...Is>(std::index_sequence<Is...>) {
#else
        [&]<size_t...Is>(std::index_sequence<Is...>) {
#endif
            if constexpr (std::is_same_v<promises_result_t<Ts...>, void>) {
                ([&]() mutable {
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
                    if constexpr (std::is_same_v<Ts, void>) {
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
        }(promises_result_index_sequence_for<Ts...>{});

        return promise;
    }

    template<typename ...Ts>
    auto all(Ts &&...promises) {
        return all(promises...);
    }

    template<typename ...Ts, typename E>
    Promise<std::tuple<Promise<Ts, E>...>, E> allSettled(Promise<Ts, E> &...promises) {
        Promise<void, E> promise;
        std::shared_ptr<size_t> remain = std::make_shared<size_t>(sizeof...(Ts));

        ([&]() {
            promises.finally([=]() mutable {
                if (--(*remain) > 0)
                    return;

                promise.resolve();
            });
        }(), ...);

        return promise.then([=]() {
            return std::tuple{promises...};
        });
    }

    template<typename ...Ts>
    auto allSettled(Ts &&...promises) {
        return allSettled(promises...);
    }

    template<typename ...Ts, typename E>
    auto any(Promise<Ts, E> &...promises) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        std::shared_ptr<size_t> remain = std::make_shared<size_t>(sizeof...(Ts));
        std::shared_ptr<std::list<E>> reasons = std::make_shared<std::list<E>>();
        Promise<std::conditional_t<Same, T, std::any>, std::list<E>> promise;

        ([&]() {
            if constexpr (std::is_same_v<Ts, void>) {
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
            if constexpr (std::is_same_v<Ts, void>) {
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
