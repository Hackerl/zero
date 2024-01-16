#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <memory>
#include <any>
#include <list>
#include <tl/expected.hpp>
#include <zero/detail/type_traits.h>

namespace zero::async::promise {
    template<typename T, typename E = std::nullptr_t>
    class Promise;

    template<typename T, typename E = std::nullptr_t>
    using PromisePtr = std::shared_ptr<Promise<T, E>>;

    template<typename T, typename E = std::nullptr_t>
    using PromiseConstPtr = std::shared_ptr<const Promise<T, E>>;

    template<typename T>
    struct promise_next_result {
        using type = T;
    };

    template<typename T, typename E>
    struct promise_next_result<PromisePtr<T, E>> {
        using type = T;
    };

    template<typename T, typename E>
    struct promise_next_result<tl::expected<T, E>> {
        using type = T;
    };

    template<typename T>
    using promise_next_result_t = typename promise_next_result<T>::type;

    template<typename T>
    struct promise_next_reason {
        using type = T;
    };

    template<typename T, typename E>
    struct promise_next_reason<PromisePtr<T, E>> {
        using type = E;
    };

    template<typename T, typename E>
    struct promise_next_reason<tl::expected<T, E>> {
        using type = E;
    };

    template<typename T>
    struct promise_next_reason<tl::unexpected<T>> {
        using type = T;
    };

    template<typename T>
    using promise_next_reason_t = typename promise_next_reason<T>::type;

    template<typename T>
    inline constexpr bool is_promise_ptr_v = false;

    template<typename T, typename E>
    inline constexpr bool is_promise_ptr_v<PromisePtr<T, E>> = true;

    enum State {
        PENDING,
        FULFILLED,
        REJECTED
    };

    template<typename T, typename E = std::nullptr_t>
    PromisePtr<T, E> make() {
        return std::make_shared<Promise<T, E>>();
    }

    template<typename T, typename E>
    class Promise {
    public:
        using value_type = T;
        using error_type = E;

        Promise() : mStatus(PENDING) {
        }

        Promise(const Promise &) = delete;
        Promise &operator=(const Promise &) = delete;

        template<typename... Ts>
        void resolve(Ts &&... args) {
            if (mStatus != PENDING)
                return;

            mStatus = FULFILLED;
            mResult = std::make_unique<tl::expected<T, E>>(std::forward<Ts>(args)...);

            for (const auto &[onFulfilled, onRejected]: std::exchange(mTriggers, {}))
                onFulfilled();
        }

        template<typename... Ts>
        void reject(Ts &&... args) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mResult = std::make_unique<tl::expected<T, E>>(tl::unexpected(std::forward<Ts>(args)...));

            for (const auto &[onFulfilled, onRejected]: std::exchange(mTriggers, {}))
                onRejected();
        }

        template<typename F>
        auto then(F &&f) {
            using Next = detail::callable_result_t<F>;
            using NextResult = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, T, promise_next_result_t<Next>>;
            using NextReason = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, promise_next_reason_t<Next>, E>;

            auto promise = make<NextResult, NextReason>();

            auto onFulfilledTrigger = [=, f = std::forward<F>(f)] {
                if constexpr (std::is_void_v<Next>) {
                    if constexpr (std::is_void_v<T>) {
                        f();
                    }
                    else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, std::as_const(mResult->value()));
                    }
                    else {
                        f(std::as_const(mResult->value()));
                    }

                    promise->resolve();
                }
                else {
                    Next next = [&] {
                        if constexpr (std::is_void_v<T>) {
                            return f();
                        }
                        else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, std::as_const(mResult->value()));
                        }
                        else {
                            return f(std::as_const(mResult->value()));
                        }
                    }();

                    if constexpr (!is_promise_ptr_v<Next>) {
                        if constexpr (detail::is_specialization<Next, tl::expected>) {
                            static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_void_v<NextResult>) {
                                    promise->resolve();
                                }
                                else {
                                    promise->resolve(std::move(next.value()));
                                }
                            }
                            else {
                                promise->reject(std::move(next.error()));
                            }
                        }
                        else if constexpr (detail::is_specialization<Next, tl::unexpected>) {
                            promise->reject(std::move(next.error()));
                        }
                        else {
                            promise->resolve(std::move(next));
                        }
                    }
                    else {
                        static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                        if constexpr (std::is_void_v<NextResult>) {
                            next->then(
                                [=] {
                                    promise->resolve();
                                },
                                [=](const E &reason) {
                                    promise->reject(reason);
                                }
                            );
                        }
                        else {
                            next->then(
                                [=](const NextResult &nextResult) {
                                    promise->resolve(nextResult);
                                },
                                [=](const E &reason) {
                                    promise->reject(reason);
                                }
                            );
                        }
                    }
                }
            };

            auto onRejectedTrigger = [=] {
                static_assert(std::is_same_v<NextReason, E>);
                promise->reject(std::as_const(mResult->error()));
            };

            switch (mStatus) {
            case FULFILLED:
                onFulfilledTrigger();
                break;

            case REJECTED:
                onRejectedTrigger();
                break;

            case PENDING:
                mTriggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
                break;

            default:
                std::abort();
            }

            return promise;
        }

        template<typename F>
        auto fail(F &&f) {
            using Next = detail::callable_result_t<F>;
            using NextResult = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, T, promise_next_result_t<Next>>;
            using NextReason = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, promise_next_reason_t<Next>, E>;

            auto promise = make<NextResult, NextReason>();

            auto onFulfilledTrigger = [=] {
                static_assert(std::is_same_v<NextResult, T>);

                if constexpr (std::is_void_v<T>) {
                    promise->resolve();
                }
                else {
                    promise->resolve(std::as_const(mResult->value()));
                }
            };

            auto onRejectedTrigger = [=, f = std::forward<F>(f)] {
                if constexpr (std::is_void_v<Next>) {
                    if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, std::as_const(mResult->error()));
                    }
                    else {
                        f(std::as_const(mResult->error()));
                    }

                    promise->resolve();
                }
                else {
                    Next next = [&] {
                        if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, std::as_const(mResult->error()));
                        }
                        else {
                            return f(std::as_const(mResult->error()));
                        }
                    }();

                    if constexpr (!is_promise_ptr_v<Next>) {
                        if constexpr (detail::is_specialization<Next, tl::expected>) {
                            static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_void_v<NextResult>) {
                                    promise->resolve();
                                }
                                else {
                                    promise->resolve(std::move(next.value()));
                                }
                            }
                            else {
                                promise->reject(std::move(next.error()));
                            }
                        }
                        else if constexpr (detail::is_specialization<Next, tl::unexpected>) {
                            promise->reject(std::move(next.value()));
                        }
                        else {
                            promise->resolve(std::move(next));
                        }
                    }
                    else {
                        static_assert(std::is_same_v<promise_next_reason_t<Next>, E>);

                        if constexpr (std::is_void_v<NextResult>) {
                            next->then(
                                [=] {
                                    promise->resolve();
                                },
                                [=](const E &reason) {
                                    promise->reject(reason);
                                }
                            );
                        }
                        else {
                            next->then(
                                [=](const NextResult &nextResult) {
                                    promise->resolve(nextResult);
                                },
                                [=](const E &reason) {
                                    promise->reject(reason);
                                }
                            );
                        }
                    }
                }
            };

            switch (mStatus) {
            case FULFILLED:
                onFulfilledTrigger();
                break;

            case REJECTED:
                onRejectedTrigger();
                break;

            case PENDING:
                mTriggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
                break;

            default:
                std::abort();
            }

            return promise;
        }

        template<typename F>
        auto finally(F &&f) {
            auto promise = make<value_type, error_type>();

            auto onFulfilledTrigger = [=] {
                f();

                if constexpr (std::is_void_v<T>) {
                    promise->resolve();
                }
                else {
                    promise->resolve(std::as_const(mResult->value()));
                }
            };

            auto onRejectedTrigger = [=] {
                f();
                promise->reject(std::as_const(mResult->error()));
            };

            switch (mStatus) {
            case FULFILLED:
                onFulfilledTrigger();
                break;

            case REJECTED:
                onRejectedTrigger();
                break;

            case PENDING:
                mTriggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
                break;

            default:
                std::abort();
            }

            return promise;
        }

        template<typename F1, typename F2>
        auto then(F1 &&f1, F2 &&f2) {
            return then(std::forward<F1>(f1))->fail(std::forward<F2>(f2));
        }

        [[nodiscard]] State status() const {
            return mStatus;
        }

        [[nodiscard]] std::add_lvalue_reference_t<std::add_const_t<T>> value() const {
            return mResult->value();
        }

        [[nodiscard]] const E &reason() const {
            return mResult->error();
        }

        [[nodiscard]] const tl::expected<T, E> &result() const {
            return *mResult;
        }

        tl::expected<T, E> &result() {
            return *mResult;
        }

    private:
        State mStatus;
        std::unique_ptr<tl::expected<T, E>> mResult;
        std::list<std::pair<std::function<void()>, std::function<void()>>> mTriggers;

        template<std::size_t... Is, typename... Ts, typename... Error>
        friend PromisePtr<std::tuple<tl::expected<Ts, Error>...>>
        allSettled(std::index_sequence<Is...>, const PromisePtr<Ts, Error> &... promises);
    };

    template<typename... Ts>
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

    template<std::size_t Count, std::size_t Index, std::size_t N, std::size_t... Is>
    struct promises_result_index_sequence : promises_result_index_sequence<Count - 1, Index + N, Is..., Index> {
    };

    template<std::size_t Index, std::size_t N, std::size_t... Is>
    struct promises_result_index_sequence<0, Index, N, Is...> : std::index_sequence<N, Is...> {
    };

    template<typename... Ts>
    using promises_result_index_sequence_for = promises_result_index_sequence<
        sizeof...(Ts),
        0,
        std::is_void_v<Ts> ? 0 : 1 ...
    >;

    template<typename T, typename E, typename F>
    PromisePtr<T, E> chain(F &&f) {
        auto promise = make<T, E>();
        f(promise);
        return promise;
    }

    template<typename T, typename E, typename U>
    PromisePtr<T, E> reject(U &&reason) {
        auto promise = make<T, E>();
        promise->reject(std::forward<U>(reason));
        return promise;
    }

    template<typename T, typename E, std::enable_if_t<std::is_void_v<T>, void *>  = nullptr>
    PromisePtr<T, E> resolve() {
        auto promise = make<T, E>();
        promise->resolve();
        return promise;
    }

    template<typename T, typename E, typename U, std::enable_if_t<!std::is_void_v<T>, void *>  = nullptr>
    PromisePtr<T, E> resolve(U &&result) {
        auto promise = make<T, E>();
        promise->resolve(std::forward<U>(result));
        return promise;
    }

    template<std::size_t... Is, typename... Ts, typename E>
    PromisePtr<promises_result_t<Ts...>, E> all(std::index_sequence<Is...>, const PromisePtr<Ts, E> &... promises) {
        auto promise = make<promises_result_t<Ts...>, E>();
        const auto remain = std::make_shared<std::size_t>(sizeof...(promises));

        if constexpr (std::is_void_v<promises_result_t<Ts...>>) {
            ([&] {
                promises->then(
                    [=] {
                        if (--*remain > 0)
                            return;

                        promise->resolve();
                    },
                    [=](const E &reason) {
                        promise->reject(reason);
                    }
                );
            }(), ...);
        }
        else {
            const auto results = std::make_shared<promises_result_t<Ts...>>();

            ([&] {
                if constexpr (std::is_void_v<Ts>) {
                    promises->then(
                        [=] {
                            if (--*remain > 0)
                                return;

                            promise->resolve(std::move(*results));
                        },
                        [=](const E &reason) {
                            promise->reject(reason);
                        }
                    );
                }
                else {
                    promises->then(
                        [=](const Ts &result) {
                            std::get<Is>(*results) = result;

                            if (--*remain > 0)
                                return;

                            promise->resolve(std::move(*results));
                        },
                        [=](const E &reason) {
                            promise->reject(reason);
                        }
                    );
                }
            }(), ...);
        }

        return promise;
    }

    template<typename... Ts, typename E>
    PromisePtr<promises_result_t<Ts...>, E> all(const PromisePtr<Ts, E> &... promises) {
        return all(promises_result_index_sequence_for<Ts...>{}, promises...);
    }

    template<std::size_t... Is, typename... Ts, typename... E>
    PromisePtr<std::tuple<tl::expected<Ts, E>...>>
    allSettled(std::index_sequence<Is...>, const PromisePtr<Ts, E> &... promises) {
        using T = std::tuple<tl::expected<Ts, E>...>;

        auto promise = make<T>();

        const auto results = std::make_shared<T>();
        const auto remain = std::make_shared<std::size_t>(sizeof...(Ts));

        ([&] {
            promises->finally([=, &result = promises->mResult] {
                std::get<Is>(*results) = *result;

                if (--*remain > 0)
                    return;

                promise->resolve(std::move(*results));
            });
        }(), ...);

        return promise;
    }

    template<typename... Ts, typename... E>
    PromisePtr<std::tuple<tl::expected<Ts, E>...>> allSettled(const PromisePtr<Ts, E> &... promises) {
        return allSettled(std::index_sequence_for<Ts...>{}, promises...);
    }

    template<typename... Ts, typename E>
    auto any(const PromisePtr<Ts, E> &... promises) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        auto promise = make<std::conditional_t<Same, T, std::any>, std::list<E>>();

        const auto remain = std::make_shared<std::size_t>(sizeof...(Ts));
        const auto reasons = std::make_shared<std::list<E>>();

        ([&] {
            if constexpr (std::is_void_v<Ts>) {
                promises->then(
                    [=] {
                        if constexpr (Same)
                            promise->resolve();
                        else
                            promise->resolve(std::any{});
                    },
                    [=](const E &reason) {
                        reasons->push_front(reason);

                        if (--*remain > 0)
                            return;

                        promise->reject(std::move(*reasons));
                    }
                );
            }
            else {
                promises->then(
                    [=](const Ts &result) {
                        promise->resolve(result);
                    },
                    [=](const E &reason) {
                        reasons->push_front(reason);

                        if (--*remain > 0)
                            return;

                        promise->reject(std::move(*reasons));
                    }
                );
            }
        }(), ...);

        return promise;
    }

    template<typename... Ts, typename E>
    auto race(const PromisePtr<Ts, E> &... promises) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        auto promise = make<std::conditional_t<Same, T, std::any>, E>();

        ([&] {
            if constexpr (std::is_void_v<Ts>) {
                promises->then(
                    [=] {
                        if constexpr (Same)
                            promise->resolve();
                        else
                            promise->resolve(std::any{});
                    },
                    [=](const E &reason) {
                        promise->reject(reason);
                    }
                );
            }
            else {
                promises->then(
                    [=](const Ts &result) {
                        promise->resolve(result);
                    },
                    [=](const E &reason) {
                        promise->reject(reason);
                    }
                );
            }
        }(), ...);

        return promise;
    }
}

#endif //ZERO_PROMISE_H
