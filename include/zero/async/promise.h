#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <memory>
#include <any>
#include <mutex>
#include <cassert>
#include <optional>
#include <stdexcept>
#include <fmt/core.h>
#include <tl/expected.hpp>
#include <zero/atomic/event.h>
#include <zero/detail/type_traits.h>
#include <range/v3/algorithm.hpp>

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

    enum class State {
        PENDING,
        ONLY_CALLBACK,
        ONLY_RESULT,
        DONE
    };

    template<typename T, typename E>
    struct Core {
        atomic::Event event{true};
        std::atomic<State> status{State::PENDING};
        std::optional<tl::expected<T, E>> result;
        std::function<void(tl::expected<T, E>)> callback;

        void trigger() {
            assert(status == State::DONE);
            std::exchange(callback, nullptr)(std::move(*result));
        }

        bool hasResult() const {
            const State state = status;
            return state == State::ONLY_RESULT || state == State::DONE;
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
            assert(mCore->status != State::ONLY_RESULT);
            assert(mCore->status != State::DONE);

            if constexpr (std::is_void_v<T>) {
                static_assert(sizeof...(Ts) == 0);
                mCore->result.emplace();
            }
            else {
                static_assert(sizeof...(Ts) > 0);
                mCore->result.emplace(tl::in_place, std::forward<Ts>(args)...);
            }

            State state = mCore->status;

            if (state == State::PENDING && mCore->status.compare_exchange_strong(state, State::ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != State::ONLY_CALLBACK || !mCore->status.compare_exchange_strong(state, State::DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->event.set();
            mCore->trigger();
        }

        template<typename... Ts>
        void reject(Ts &&... args) {
            static_assert(sizeof...(Ts) > 0);
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->status != State::ONLY_RESULT);
            assert(mCore->status != State::DONE);

            mCore->result.emplace(tl::unexpected<E>(std::forward<Ts>(args)...));
            State state = mCore->status;

            if (state == State::PENDING && mCore->status.compare_exchange_strong(state, State::ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != State::ONLY_CALLBACK || !mCore->status.compare_exchange_strong(state, State::DONE))
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

        [[nodiscard]] const tl::expected<T, E> &result() const & {
            assert(mCore);
            assert(mCore->result);
            return *mCore->result;
        }

        tl::expected<T, E> &&result() && {
            assert(mCore);
            assert(mCore->result);
            return std::move(*mCore->result);
        }

        [[nodiscard]] const tl::expected<T, E> &&result() const && {
            assert(mCore);
            assert(mCore->result);
            return std::move(*mCore->result);
        }

        [[nodiscard]] tl::expected<void, std::error_code>
        wait(const std::optional<std::chrono::milliseconds> timeout = std::nullopt) const {
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
        void setCallback(F &&f) {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->status != State::ONLY_CALLBACK);
            assert(mCore->status != State::DONE);
            mCore->callback = std::forward<F>(f);

            State state = mCore->status;

            if (state == State::PENDING && mCore->status.compare_exchange_strong(state, State::ONLY_CALLBACK))
                return;

            if (state != State::ONLY_RESULT || !mCore->status.compare_exchange_strong(state, State::DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->trigger();
        }

        template<typename F>
        auto then(F &&f) && {
            using Next = detail::callable_result_t<F>;
            using NextValue = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, T, future_next_value_t<Next>>;
            using NextError = std::conditional_t<
                detail::is_specialization<Next, tl::unexpected>, future_next_error_t<Next>, E>;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->status != State::ONLY_CALLBACK);
            assert(mCore->status != State::DONE);

            const auto promise = std::make_shared<Promise<NextValue, NextError>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> result) {
                if (!result) {
                    static_assert(std::is_same_v<NextError, E>);
                    promise->reject(std::move(result).error());
                    return;
                }

                if constexpr (std::is_void_v<Next>) {
                    if constexpr (std::is_void_v<T>) {
                        f();
                    }
                    else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, *std::move(result));
                    }
                    else {
                        f(*std::move(result));
                    }

                    promise->resolve();
                }
                else {
                    Next next = [&] {
                        if constexpr (std::is_void_v<T>) {
                            return f();
                        }
                        else if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, *std::move(result));
                        }
                        else {
                            return f(*std::move(result));
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
            });

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
            assert(!mCore->callback);
            assert(mCore->status != State::ONLY_CALLBACK);
            assert(mCore->status != State::DONE);

            const auto promise = std::make_shared<Promise<NextValue, NextError>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> result) {
                if (result) {
                    static_assert(std::is_same_v<NextValue, T>);

                    if constexpr (std::is_void_v<T>) {
                        promise->resolve();
                    }
                    else {
                        promise->resolve(*std::move(result));
                    }

                    return;
                }

                if constexpr (std::is_void_v<Next>) {
                    if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                        std::apply(f, std::move(result).error());
                    }
                    else {
                        f(std::move(result).error());
                    }

                    promise->resolve();
                }
                else {
                    Next next = [&] {
                        if constexpr (detail::is_applicable_v<detail::callable_type_t<F>, T>) {
                            return std::apply(f, std::move(result).error());
                        }
                        else {
                            return f(std::move(result).error());
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
            });

            return promise->getFuture();
        }

        template<typename F>
        auto finally(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->status != State::ONLY_CALLBACK);
            assert(mCore->status != State::DONE);

            const auto promise = std::make_shared<Promise<T, E>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> result) {
                if (!result) {
                    f();
                    promise->reject(std::move(result).error());
                    return;
                }

                f();

                if constexpr (std::is_void_v<T>) {
                    promise->resolve();
                }
                else {
                    promise->resolve(*std::move(result));
                }
            });

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
        detail::all_same_v<Ts...>,
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

    template<typename T, typename E, std::enable_if_t<std::is_void_v<T>>* = nullptr>
    Future<T, E> resolve() {
        Promise<T, E> promise;
        promise.resolve();
        return promise.getFuture();
    }

    template<
        typename T,
        typename E,
        typename... Ts,
        std::enable_if_t<!std::is_void_v<T> && (sizeof...(Ts) > 0)>* = nullptr
    >
    Future<T, E> resolve(Ts &&... args) {
        Promise<T, E> promise;
        promise.resolve(std::forward<Ts>(args)...);
        return promise.getFuture();
    }

    template<
        typename I,
        typename S,
        std::enable_if_t<ranges::input_iterator<I>>* = nullptr,
        std::enable_if_t<ranges::sentinel_for<S, I>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::iter_value_t<I>, Future>>* = nullptr
    >
    auto all(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        if constexpr (std::is_void_v<T>) {
            struct Context {
                explicit Context(const std::size_t n) : count(n) {
                }

                Promise<void, E> promise;
                std::atomic<std::size_t> count;
                std::atomic_flag flag = ATOMIC_FLAG_INIT;
            };

            const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

            while (first != last) {
                (*first++).setCallback([=](tl::expected<T, E> result) {
                    if (!result) {
                        if (!ctx->flag.test_and_set())
                            ctx->promise.reject(std::move(result).error());

                        return;
                    }

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve();
                });
            }

            return ctx->promise.getFuture();
        }
        else {
            struct Context {
                explicit Context(const std::size_t n) : count(n), values(n) {
                }

                Promise<std::vector<T>, E> promise;
                std::atomic<std::size_t> count;
                std::atomic_flag flag = ATOMIC_FLAG_INIT;
                std::vector<T> values;
            };

            const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

            for (std::size_t i = 0; first != last; ++first, ++i) {
                (*first).setCallback([=](tl::expected<T, E> result) {
                    if (!result) {
                        if (!ctx->flag.test_and_set())
                            ctx->promise.reject(std::move(result).error());

                        return;
                    }

                    ctx->values[i] = *std::move(result);

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->values));
                });
            }

            return ctx->promise.getFuture();
        }
    }

    template<
        typename R,
        std::enable_if_t<ranges::range<R>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::range_value_t<R>, Future>>* = nullptr
    >
    auto all(R futures) {
        return all(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename E>
    auto all(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        if constexpr (std::is_void_v<futures_result_t<Ts...>>) {
            struct Context {
                Promise<void, E> promise;
                std::atomic<std::size_t> count{sizeof...(Ts)};
                std::atomic_flag flag = ATOMIC_FLAG_INIT;
            };

            const auto ctx = std::make_shared<Context>();

            ([&] {
                futures.setCallback([=](tl::expected<Ts, E> result) {
                    if (!result) {
                        if (!ctx->flag.test_and_set())
                            ctx->promise.reject(std::move(result).error());

                        return;
                    }

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve();
                });
            }(), ...);

            return ctx->promise.getFuture();
        }
        else {
            struct Context {
                Promise<futures_result_t<Ts...>, E> promise;
                std::atomic<std::size_t> count{sizeof...(Ts)};
                std::atomic_flag flag = ATOMIC_FLAG_INIT;
                futures_result_t<Ts...> values;
            };

            const auto ctx = std::make_shared<Context>();

            ([&] {
                futures.setCallback([=](tl::expected<Ts, E> result) {
                    if (!result) {
                        if (!ctx->flag.test_and_set())
                            ctx->promise.reject(std::move(result).error());

                        return;
                    }

                    if constexpr (std::is_void_v<Ts>) {
                        if (--ctx->count > 0)
                            return;

                        ctx->promise.resolve(std::move(ctx->values));
                    }
                    else {
                        std::get<Is>(ctx->values) = *std::move(result);

                        if (--ctx->count > 0)
                            return;

                        ctx->promise.resolve(std::move(ctx->values));
                    }
                });
            }(), ...);

            return ctx->promise.getFuture();
        }
    }

    template<typename... Ts, typename E>
    auto all(Future<Ts, E>... futures) {
        return all(futures_result_index_sequence_for<Ts...>{}, std::move(futures)...);
    }

    template<
        typename I,
        typename S,
        std::enable_if_t<ranges::input_iterator<I>>* = nullptr,
        std::enable_if_t<ranges::sentinel_for<S, I>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::iter_value_t<I>, Future>>* = nullptr
    >
    auto allSettled(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count(n), results(n) {
            }

            Promise<std::vector<tl::expected<T, E>>> promise;
            std::atomic<std::size_t> count;
            std::vector<tl::expected<T, E>> results;
        };

        const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

        for (std::size_t i = 0; first != last; ++first, ++i) {
            (*first).setCallback([=](tl::expected<T, E> result) {
                if (!result) {
                    ctx->results[i] = tl::unexpected(std::move(result).error());

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->results));
                    return;
                }

                if constexpr (std::is_void_v<T>) {
                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->results));
                }
                else {
                    ctx->results[i] = *std::move(result);

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->results));
                }
            });
        }

        return ctx->promise.getFuture();
    }

    template<
        typename R,
        std::enable_if_t<ranges::range<R>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::range_value_t<R>, Future>>* = nullptr
    >
    auto allSettled(R futures) {
        return allSettled(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename... Es>
    auto allSettled(std::index_sequence<Is...>, Future<Ts, Es>... futures) {
        using T = std::conditional_t<
            detail::all_same_v<Ts...>,
            std::array<tl::expected<detail::first_element_t<Ts...>, detail::first_element_t<Es...>>, sizeof...(Ts)>,
            std::tuple<tl::expected<Ts, Es>...>
        >;

        struct Context {
            Promise<T> promise;
            std::atomic<std::size_t> count{sizeof...(Ts)};
            T results{};
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](tl::expected<Ts, Es> result) {
                if (!result) {
                    std::get<Is>(ctx->results) = tl::unexpected(std::move(result).error());

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->results));
                    return;
                }

                if constexpr (std::is_void_v<Ts>) {
                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->results));
                }
                else {
                    std::get<Is>(ctx->results) = *std::move(result);

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(std::move(ctx->results));
                }
            });
        }(), ...);

        return ctx->promise.getFuture();
    }

    template<typename... Ts, typename... Es>
    auto allSettled(Future<Ts, Es>... promises) {
        return allSettled(std::index_sequence_for<Ts...>{}, std::move(promises)...);
    }

    template<
        typename I,
        typename S,
        std::enable_if_t<ranges::input_iterator<I>>* = nullptr,
        std::enable_if_t<ranges::sentinel_for<S, I>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::iter_value_t<I>, Future>>* = nullptr
    >
    auto any(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count(n), errors(n) {
            }

            Promise<T, std::vector<E>> promise;
            std::atomic<std::size_t> count;
            std::atomic_flag flag = ATOMIC_FLAG_INIT;
            std::vector<E> errors;
        };

        const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

        for (std::size_t i = 0; first != last; ++first, ++i) {
            (*first).setCallback([=](tl::expected<T, E> result) {
                if (!result) {
                    ctx->errors[i] = std::move(result).error();

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.reject(std::move(ctx->errors));
                    return;
                }

                if (!ctx->flag.test_and_set()) {
                    if constexpr (std::is_void_v<T>)
                        ctx->promise.resolve();
                    else
                        ctx->promise.resolve(*std::move(result));
                }
            });
        }

        return ctx->promise.getFuture();
    }

    template<
        typename R,
        std::enable_if_t<ranges::range<R>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::range_value_t<R>, Future>>* = nullptr
    >
    auto any(R futures) {
        return any(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename E>
    auto any(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        constexpr bool Same = detail::all_same_v<Ts...>;

        struct Context {
            Promise<
                std::conditional_t<
                    Same,
                    detail::first_element_t<Ts...>,
                    std::any
                >,
                std::array<E, sizeof...(Ts)>
            >
            promise;
            std::atomic<std::size_t> count{sizeof...(Ts)};
            std::atomic_flag flag = ATOMIC_FLAG_INIT;
            std::array<E, sizeof...(Ts)> errors;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](tl::expected<Ts, E> result) {
                if (!result) {
                    std::get<Is>(ctx->errors) = std::move(result).error();

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.reject(std::move(ctx->errors));
                    return;
                }

                if (!ctx->flag.test_and_set()) {
                    if constexpr (std::is_void_v<Ts>) {
                        if constexpr (Same)
                            ctx->promise.resolve();
                        else
                            ctx->promise.resolve(std::any{});
                    }
                    else {
                        ctx->promise.resolve(*std::move(result));
                    }
                }
            });
        }(), ...);

        return ctx->promise.getFuture();
    }

    template<typename... Ts, typename E>
    auto any(Future<Ts, E>... futures) {
        return any(std::index_sequence_for<Ts...>{}, std::move(futures)...);
    }

    template<
        typename I,
        typename S,
        std::enable_if_t<ranges::input_iterator<I>>* = nullptr,
        std::enable_if_t<ranges::sentinel_for<S, I>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::iter_value_t<I>, Future>>* = nullptr
    >
    auto race(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        struct Context {
            Promise<T, E> promise;
            std::atomic_flag flag = ATOMIC_FLAG_INIT;
        };

        const auto ctx = std::make_shared<Context>();

        while (first != last) {
            (*first++).setCallback([=](tl::expected<T, E> result) {
                if (!result) {
                    if (!ctx->flag.test_and_set())
                        ctx->promise.reject(std::move(result).error());

                    return;
                }

                if (!ctx->flag.test_and_set()) {
                    if constexpr (std::is_void_v<T>)
                        ctx->promise.resolve();
                    else
                        ctx->promise.resolve(*std::move(result));
                }
            });
        }

        return ctx->promise.getFuture();
    }

    template<
        typename R,
        std::enable_if_t<ranges::range<R>>* = nullptr,
        std::enable_if_t<detail::is_specialization<ranges::range_value_t<R>, Future>>* = nullptr
    >
    auto race(R futures) {
        return race(futures.begin(), futures.end());
    }

    template<typename... Ts, typename E>
    auto race(Future<Ts, E>... futures) {
        constexpr bool Same = detail::all_same_v<Ts...>;

        struct Context {
            Promise<std::conditional_t<Same, detail::first_element_t<Ts...>, std::any>, E> promise;
            std::atomic_flag flag = ATOMIC_FLAG_INIT;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](tl::expected<Ts, E> result) {
                if (!result) {
                    if (!ctx->flag.test_and_set())
                        ctx->promise.reject(std::move(result).error());

                    return;
                }

                if (!ctx->flag.test_and_set()) {
                    if constexpr (std::is_void_v<Ts>) {
                        if constexpr (Same)
                            ctx->promise.resolve();
                        else
                            ctx->promise.resolve(std::any{});
                    }
                    else {
                        ctx->promise.resolve(*std::move(result));
                    }
                }
            });
        }(), ...);

        return ctx->promise.getFuture();
    }
}

#endif //ZERO_PROMISE_H
