#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <memory>
#include <any>
#include <list>
#include <mutex>
#include <cassert>
#include <optional>
#include <stdexcept>
#include <fmt/core.h>
#include <tl/expected.hpp>
#include <zero/atomic/event.h>
#include <zero/detail/type_traits.h>

namespace zero::async::promise {
    template<typename T, typename E = std::nullptr_t>
    class Future;

    template<typename T>
    struct future_next_value {
        using type = T;
    };

    template<typename T, typename E>
    struct future_next_value<Future<T, E>> {
        using type = T;
    };

    template<typename T, typename E>
    struct future_next_value<tl::expected<T, E>> {
        using type = T;
    };

    template<typename T>
    using future_next_value_t = typename future_next_value<T>::type;

    template<typename T>
    struct future_next_error {
        using type = T;
    };

    template<typename T, typename E>
    struct future_next_error<Future<T, E>> {
        using type = E;
    };

    template<typename T, typename E>
    struct future_next_error<tl::expected<T, E>> {
        using type = E;
    };

    template<typename T>
    struct future_next_error<tl::unexpected<T>> {
        using type = T;
    };

    template<typename T>
    using future_next_error_t = typename future_next_error<T>::type;

    template<typename T>
    inline constexpr bool is_future_v = false;

    template<typename T, typename E>
    inline constexpr bool is_future_v<Future<T, E>> = true;

    enum State {
        PENDING,
        ONLY_CALLBACK,
        ONLY_RESULT,
        DONE
    };

    template<typename T, typename E>
    struct Core {
        atomic::Event event{true};
        std::atomic<State> status{PENDING};
        std::optional<tl::expected<T, E>> result;
        std::function<void()> onSuccess;
        std::function<void()> onFailure;

        void trigger() {
            assert(status == DONE);
            assert(result);
            assert(onSuccess);
            assert(onFailure);

            if (result->has_value())
                std::exchange(onSuccess, nullptr)();
            else
                std::exchange(onFailure, nullptr)();
        }

        bool hasResult() const {
            const State state = status;
            return state == ONLY_RESULT || state == DONE;
        }
    };

    template<typename T, typename E = std::nullptr_t>
    class Promise {
    public:
        using value_type = T;
        using error_type = E;

        Promise() : mRetrieved(false), mCore(std::make_shared<Core<T, E>>()) {
        }

        Promise(Promise &&rhs) noexcept
            : mRetrieved(std::exchange(rhs.mRetrieved, false)), mCore(std::move(rhs.mCore)) {
        }

        Promise &operator=(Promise &&rhs) noexcept {
            mRetrieved = std::exchange(rhs.mRetrieved, false);
            mCore = std::move(rhs.mCore);
            return *this;
        }

        [[nodiscard]] bool valid() const {
            return mCore != nullptr;
        }

        [[nodiscard]] bool isFulfilled() const {
            if (mCore)
                return mCore->hasResult();

            return true;
        }

        Future<T, E> getFuture() {
            assert(mCore);
            assert(!mRetrieved);

            if (mRetrieved)
                throw std::logic_error("future already retrieved");

            mRetrieved = true;
            return Future<T, E>{mCore};
        }

        template<typename... Ts>
        void resolve(Ts &&... args) {
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->status != ONLY_RESULT);
            assert(mCore->status != DONE);

            if constexpr (std::is_void_v<T>)
                mCore->result.emplace();
            else
                mCore->result.emplace(tl::in_place, std::forward<Ts>(args)...);

            State state = mCore->status;

            if (state == PENDING && mCore->status.compare_exchange_strong(state, ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != ONLY_CALLBACK || !mCore->status.compare_exchange_strong(state, DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->event.set();
            mCore->trigger();
        }

        template<typename... Ts>
        void reject(Ts &&... args) {
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->status != ONLY_RESULT);
            assert(mCore->status != DONE);

            mCore->result.emplace(tl::unexpected<E>(std::forward<Ts>(args)...));
            State state = mCore->status;

            if (state == PENDING && mCore->status.compare_exchange_strong(state, ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != ONLY_CALLBACK || !mCore->status.compare_exchange_strong(state, DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->event.set();
            mCore->trigger();
        }

    protected:
        bool mRetrieved;
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename T, typename E>
    class Future {
    public:
        using value_type = T;
        using error_type = E;

        explicit Future(std::shared_ptr<Core<T, E>> core) : mCore(std::move(core)) {
        }

        Future(Future &&rhs) noexcept : mCore(std::move(rhs.mCore)) {
        }

        Future &operator=(Future &&rhs) noexcept {
            mCore = std::move(rhs.mCore);
            return *this;
        }

        [[nodiscard]] bool valid() const {
            return mCore != nullptr;
        }

        [[nodiscard]] bool isReady() const {
            assert(mCore);
            return mCore->hasResult();
        }

        tl::expected<T, E> &result() & {
            assert(mCore);
            assert(mCore->result);
            return *mCore->result;
        }

        const tl::expected<T, E> &result() const & {
            assert(mCore);
            assert(mCore->result);
            return *mCore->result;
        }

        tl::expected<T, E> &&result() && {
            assert(mCore);
            assert(mCore->result);
            return std::move(*mCore->result);
        }

        const tl::expected<T, E> &&result() const && {
            assert(mCore);
            assert(mCore->result);
            return std::move(*mCore->result);
        }

        [[nodiscard]] tl::expected<void, std::error_code>
        wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt) const {
            assert(mCore);

            if (mCore->hasResult())
                return {};

            return mCore->event.wait(timeout);
        }

        tl::expected<T, E> get() && {
            if (const auto result = wait(); !result)
                throw std::system_error(result.error());

            return std::move(*mCore->result);
        }

        template<typename F>
        auto then(F &&f) && {
            using Next = detail::callable_result_t<F>;
            using NextValue = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, T, future_next_value_t<Next>>;
            using NextError = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, future_next_error_t<Next>, E>;

            assert(mCore);
            assert(!mCore->onSuccess);
            assert(!mCore->onFailure);
            assert(mCore->status != ONLY_CALLBACK);
            assert(mCore->status != DONE);

            const auto promise = std::make_shared<Promise<NextValue, NextError>>();

            mCore->onSuccess = [=, f = std::forward<F>(f), &result = mCore->result] {
                if constexpr (std::is_void_v<Next>) {
                    if constexpr (std::is_void_v<T>) {
                        f();
                    }
                    else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, *std::move(*result));
                    }
                    else {
                        f(*std::move(*result));
                    }

                    promise->resolve();
                }
                else {
                    Next next = [&] {
                        if constexpr (std::is_void_v<T>) {
                            return f();
                        }
                        else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, *std::move(*result));
                        }
                        else {
                            return f(*std::move(*result));
                        }
                    }();

                    if constexpr (!is_future_v<Next>) {
                        if constexpr (detail::is_specialization<Next, tl::expected>) {
                            static_assert(std::is_same_v<future_next_error_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_void_v<NextValue>) {
                                    promise->resolve();
                                }
                                else {
                                    promise->resolve(*std::move(next));
                                }
                            }
                            else {
                                promise->reject(std::move(next).error());
                            }
                        }
                        else if constexpr (detail::is_specialization<Next, tl::unexpected>) {
                            promise->reject(std::move(next).value());
                        }
                        else {
                            promise->resolve(std::move(next));
                        }
                    }
                    else {
                        static_assert(std::is_same_v<future_next_error_t<Next>, E>);

                        if constexpr (std::is_void_v<NextValue>) {
                            std::move(next).then(
                                [=] {
                                    promise->resolve();
                                },
                                [=](E error) {
                                    promise->reject(std::move(error));
                                }
                            );
                        }
                        else {
                            std::move(next).then(
                                [=](NextValue value) {
                                    promise->resolve(std::move(value));
                                },
                                [=](E error) {
                                    promise->reject(std::move(error));
                                }
                            );
                        }
                    }
                }
            };

            mCore->onFailure = [=, &result = mCore->result] {
                static_assert(std::is_same_v<NextError, E>);
                promise->reject(std::move(*result).error());
            };

            State state = mCore->status;

            if (state == PENDING && mCore->status.compare_exchange_strong(state, ONLY_CALLBACK))
                return promise->getFuture();

            if (state != ONLY_RESULT || !mCore->status.compare_exchange_strong(state, DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->trigger();
            return promise->getFuture();
        }

        template<typename F>
        auto fail(F &&f) && {
            using Next = detail::callable_result_t<F>;
            using NextValue = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, T, future_next_value_t<Next>>;
            using NextError = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, future_next_error_t<Next>, E>;

            assert(mCore);
            assert(!mCore->onSuccess);
            assert(!mCore->onFailure);
            assert(mCore->status != ONLY_CALLBACK);
            assert(mCore->status != DONE);

            const auto promise = std::make_shared<Promise<NextValue, NextError>>();

            mCore->onSuccess = [=, &result = mCore->result] {
                static_assert(std::is_same_v<NextValue, T>);

                if constexpr (std::is_void_v<T>) {
                    promise->resolve();
                }
                else {
                    promise->resolve(*std::move(*result));
                }
            };

            mCore->onFailure = [=, f = std::forward<F>(f), &result = mCore->result] {
                if constexpr (std::is_void_v<Next>) {
                    if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, std::move(*result).error());
                    }
                    else {
                        f(std::move(*result).error());
                    }

                    promise->resolve();
                }
                else {
                    Next next = [&] {
                        if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, std::move(*result).error());
                        }
                        else {
                            return f(std::move(*result).error());
                        }
                    }();

                    if constexpr (!is_future_v<Next>) {
                        if constexpr (detail::is_specialization<Next, tl::expected>) {
                            static_assert(std::is_same_v<future_next_error_t<Next>, E>);

                            if (next) {
                                if constexpr (std::is_void_v<NextValue>) {
                                    promise->resolve();
                                }
                                else {
                                    promise->resolve(*std::move(next));
                                }
                            }
                            else {
                                promise->reject(std::move(next).error());
                            }
                        }
                        else if constexpr (detail::is_specialization<Next, tl::unexpected>) {
                            promise->reject(std::move(next).value());
                        }
                        else {
                            promise->resolve(std::move(next));
                        }
                    }
                    else {
                        static_assert(std::is_same_v<future_next_error_t<Next>, E>);

                        if constexpr (std::is_void_v<NextValue>) {
                            std::move(next).then(
                                [=] {
                                    promise->resolve();
                                },
                                [=](E error) {
                                    promise->reject(std::move(error));
                                }
                            );
                        }
                        else {
                            std::move(next).then(
                                [=](NextValue value) {
                                    promise->resolve(std::move(value));
                                },
                                [=](E error) {
                                    promise->reject(std::move(error));
                                }
                            );
                        }
                    }
                }
            };

            State state = mCore->status;

            if (state == PENDING && mCore->status.compare_exchange_strong(state, ONLY_CALLBACK))
                return promise->getFuture();

            if (state != ONLY_RESULT || !mCore->status.compare_exchange_strong(state, DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->trigger();
            return promise->getFuture();
        }

        template<typename F>
        auto finally(F &&f) && {
            assert(mCore);
            assert(!mCore->onSuccess);
            assert(!mCore->onFailure);
            assert(mCore->status != ONLY_CALLBACK);
            assert(mCore->status != DONE);

            const auto promise = std::make_shared<Promise<T, E>>();

            mCore->onSuccess = [=, &result = mCore->result] {
                f();

                if constexpr (std::is_void_v<T>) {
                    promise->resolve();
                }
                else {
                    promise->resolve(*std::move(*result));
                }
            };

            mCore->onFailure = [=, &result = mCore->result] {
                f();
                promise->reject(std::move(*result).error());
            };

            State state = mCore->status;

            if (state == PENDING && mCore->status.compare_exchange_strong(state, ONLY_CALLBACK))
                return promise->getFuture();

            if (state != ONLY_RESULT || !mCore->status.compare_exchange_strong(state, DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->trigger();
            return promise->getFuture();
        }

        template<typename F1, typename F2>
        auto then(F1 &&f1, F2 &&f2) && {
            return std::move(*this).then(std::forward<F1>(f1)).fail(std::forward<F2>(f2));
        }

    private:
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename... Ts>
    using futures_result_t = std::conditional_t<
        detail::is_elements_same_v<detail::first_element_t<Ts...>, Ts...>,
        std::conditional_t<
            std::is_void_v<detail::first_element_t<Ts...>>,
            void,
            std::array<detail::first_element_t<Ts...>, sizeof...(Ts)>
        >,
        decltype(
            std::tuple_cat(
                std::declval<
                    std::conditional_t<
                        std::is_void_v<Ts>,
                        std::tuple<>,
                        std::tuple<Ts>
                    >
                >()...
            )
        )
    >;

    template<std::size_t Count, std::size_t Index, std::size_t N, std::size_t... Is>
    struct futures_result_index_sequence : futures_result_index_sequence<Count - 1, Index + N, Is..., Index> {
    };

    template<std::size_t Index, std::size_t N, std::size_t... Is>
    struct futures_result_index_sequence<0, Index, N, Is...> : std::index_sequence<N, Is...> {
    };

    template<typename... Ts>
    using futures_result_index_sequence_for = futures_result_index_sequence<
        sizeof...(Ts),
        0,
        std::is_void_v<Ts> ? 0 : 1 ...
    >;

    template<typename T, typename E, typename F>
    Future<T, E> chain(F &&f) {
        Promise<T, E> promise;
        Future<T, E> future = promise.getFuture();
        f(std::move(promise));
        return future;
    }

    template<typename T, typename E, typename... Ts>
    Future<T, E> reject(Ts &&... args) {
        Promise<T, E> promise;
        promise.reject(std::forward<Ts>(args)...);
        return promise.getFuture();
    }

    template<typename T, typename E>
        requires std::is_void_v<T>
    Future<T, E> resolve() {
        Promise<T, E> promise;
        promise.resolve();
        return promise.getFuture();
    }

    template<typename T, typename E, typename... Ts>
        requires (!std::is_void_v<T> && sizeof...(Ts) > 0)
    Future<T, E> resolve(Ts &&... args) {
        Promise<T, E> promise;
        promise.resolve(std::forward<Ts>(args)...);
        return promise.getFuture();
    }

    template<std::size_t... Is, typename... Ts, typename E>
    Future<futures_result_t<Ts...>, E> all(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        const auto promise = std::make_shared<Promise<futures_result_t<Ts...>, E>>();
        const auto remain = std::make_shared<std::atomic<std::size_t>>(sizeof...(Ts));
        const auto flag = std::make_shared<std::atomic_flag>();

        if constexpr (std::is_void_v<futures_result_t<Ts...>>) {
            ([&] {
                std::move(futures).then(
                    [=] {
                        if (--*remain > 0)
                            return;

                        promise->resolve();
                    },
                    [=](E error) {
                        if (!flag->test_and_set())
                            promise->reject(std::move(error));
                    }
                );
            }(), ...);
        }
        else {
            const auto values = std::make_shared<futures_result_t<Ts...>>();

            ([&] {
                if constexpr (std::is_void_v<Ts>) {
                    std::move(futures).then(
                        [=] {
                            if (--*remain > 0)
                                return;

                            promise->resolve(std::move(*values));
                        },
                        [=](E error) {
                            if (!flag->test_and_set())
                                promise->reject(std::move(error));
                        }
                    );
                }
                else {
                    std::move(futures).then(
                        [=](Ts value) {
                            std::get<Is>(*values) = std::move(value);

                            if (--*remain > 0)
                                return;

                            promise->resolve(std::move(*values));
                        },
                        [=](E error) {
                            if (!flag->test_and_set())
                                promise->reject(std::move(error));
                        }
                    );
                }
            }(), ...);
        }

        return promise->getFuture();
    }

    template<typename... Ts, typename E>
    Future<futures_result_t<Ts...>, E> all(Future<Ts, E>... futures) {
        return all(futures_result_index_sequence_for<Ts...>{}, std::move(futures)...);
    }

    template<std::size_t... Is, typename... Ts, typename... E>
    Future<std::tuple<tl::expected<Ts, E>...>>
    allSettled(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        using T = std::tuple<tl::expected<Ts, E>...>;

        const auto promise = std::make_shared<Promise<T>>();
        const auto results = std::make_shared<T>();
        const auto remain = std::make_shared<std::atomic<std::size_t>>(sizeof...(Ts));

        ([&] {
            if constexpr (std::is_void_v<Ts>) {
                std::move(futures).then(
                    [=] {
                        if (--*remain > 0)
                            return;

                        promise->resolve(std::move(*results));
                    },
                    [=](E error) {
                        std::get<Is>(*results) = tl::unexpected(std::move(error));

                        if (--*remain > 0)
                            return;

                        promise->resolve(std::move(*results));
                    }
                );
            }
            else {
                std::move(futures).then(
                    [=](Ts value) {
                        std::get<Is>(*results) = std::move(value);

                        if (--*remain > 0)
                            return;

                        promise->resolve(std::move(*results));
                    },
                    [=](E error) {
                        std::get<Is>(*results) = tl::unexpected(std::move(error));

                        if (--*remain > 0)
                            return;

                        promise->resolve(std::move(*results));
                    }
                );
            }
        }(), ...);

        return promise->getFuture();
    }

    template<typename... Ts, typename... E>
    Future<std::tuple<tl::expected<Ts, E>...>> allSettled(Future<Ts, E>... promises) {
        return allSettled(std::index_sequence_for<Ts...>{}, std::move(promises)...);
    }

    template<typename... Ts, typename E>
    auto any(Future<Ts, E>... futures) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        const auto promise = std::make_shared<Promise<std::conditional_t<Same, T, std::any>, std::list<E>>>();
        const auto errors = std::make_shared<std::list<E>>();
        const auto flag = std::make_shared<std::atomic_flag>();
        const auto mutex = std::make_shared<std::mutex>();

        ([&] {
            if constexpr (std::is_void_v<Ts>) {
                std::move(futures).then(
                    [=] {
                        if constexpr (Same) {
                            if (!flag->test_and_set())
                                promise->resolve();
                        }
                        else {
                            if (!flag->test_and_set())
                                promise->resolve(std::any{});
                        }
                    },
                    [=](E error) {
                        {
                            std::lock_guard guard(*mutex);
                            errors->push_front(std::move(error));

                            if (errors->size() != sizeof...(Ts))
                                return;
                        }

                        promise->reject(std::move(*errors));
                    }
                );
            }
            else {
                std::move(futures).then(
                    [=](Ts value) {
                        if (!flag->test_and_set())
                            promise->resolve(std::move(value));
                    },
                    [=](E error) {
                        {
                            std::lock_guard guard(*mutex);
                            errors->push_front(std::move(error));

                            if (errors->size() != sizeof...(Ts))
                                return;
                        }

                        promise->reject(std::move(*errors));
                    }
                );
            }
        }(), ...);

        return promise->getFuture();
    }

    template<typename... Ts, typename E>
    auto race(Future<Ts, E>... futures) {
        using T = detail::first_element_t<Ts...>;
        constexpr bool Same = detail::is_elements_same_v<T, Ts...>;

        const auto promise = std::make_shared<Promise<std::conditional_t<Same, T, std::any>, E>>();
        const auto flag = std::make_shared<std::atomic_flag>();

        ([&] {
            if constexpr (std::is_void_v<Ts>) {
                std::move(futures).then(
                    [=] {
                        if constexpr (Same) {
                            if (!flag->test_and_set())
                                promise->resolve();
                        }
                        else {
                            if (!flag->test_and_set())
                                promise->resolve(std::any{});
                        }
                    },
                    [=](E error) {
                        if (!flag->test_and_set())
                            promise->reject(std::move(error));
                    }
                );
            }
            else {
                std::move(futures).then(
                    [=](Ts value) {
                        if (!flag->test_and_set())
                            promise->resolve(std::move(value));
                    },
                    [=](E error) {
                        if (!flag->test_and_set())
                            promise->reject(std::move(error));
                    }
                );
            }
        }(), ...);

        return promise->getFuture();
    }
}

#endif //ZERO_PROMISE_H
