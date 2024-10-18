#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <any>
#include <mutex>
#include <memory>
#include <cassert>
#include <utility>
#include <optional>
#include <stdexcept>
#include <functional>
#include <fmt/core.h>
#include <tl/expected.hpp>
#include <zero/atomic/event.h>
#include <zero/detail/type_traits.h>
#include <range/v3/algorithm.hpp>

namespace zero::async::promise {
    enum class State {
        PENDING,
        ONLY_CALLBACK,
        ONLY_RESULT,
        DONE
    };

    template<typename T, typename E>
    struct Core {
        atomic::Event event{true};
        std::atomic<State> state{State::PENDING};
        std::optional<tl::expected<T, E>> result;
        std::function<void(tl::expected<T, E>)> callback;

        void trigger() {
            assert(state == State::DONE);
            std::exchange(callback, nullptr)(std::move(*result));
        }

        [[nodiscard]] bool hasResult() const {
            const auto s = state.load();
            return s == State::ONLY_RESULT || s == State::DONE;
        }
    };

    template<typename T, typename E = std::nullptr_t>
    class Future;

    template<typename T, typename E = std::nullptr_t>
    class Promise {
    public:
        using value_type = T;
        using error_type = E;

        Promise() : mRetrieved{false}, mCore{std::make_shared<Core<T, E>>()} {
        }

        Promise(Promise &&rhs) noexcept
            : mRetrieved{std::exchange(rhs.mRetrieved, false)}, mCore{std::move(rhs.mCore)} {
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
            assert(mCore->state != State::ONLY_RESULT);
            assert(mCore->state != State::DONE);

            if constexpr (std::is_void_v<T>) {
                static_assert(sizeof...(Ts) == 0);
                mCore->result.emplace();
            }
            else {
                static_assert(sizeof...(Ts) > 0);
                mCore->result.emplace(tl::in_place, std::forward<Ts>(args)...);
            }

            auto state = mCore->state.load();

            if (state == State::PENDING && mCore->state.compare_exchange_strong(state, State::ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != State::ONLY_CALLBACK || !mCore->state.compare_exchange_strong(state, State::DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->event.set();
            mCore->trigger();
        }

        template<typename... Ts>
        void reject(Ts &&... args) {
            static_assert(sizeof...(Ts) > 0);
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->state != State::ONLY_RESULT);
            assert(mCore->state != State::DONE);

            mCore->result.emplace(tl::unexpected<E>(std::forward<Ts>(args)...));
            auto state = mCore->state.load();

            if (state == State::PENDING && mCore->state.compare_exchange_strong(state, State::ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != State::ONLY_CALLBACK || !mCore->state.compare_exchange_strong(state, State::DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->event.set();
            mCore->trigger();
        }

    protected:
        bool mRetrieved;
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename F, typename T>
    using callback_result_t = typename std::conditional_t<
        std::is_void_v<T>,
        std::invoke_result<F>,
        std::invoke_result<F, T>
    >::type;

    template<typename T, typename E>
    class Future {
    public:
        using value_type = T;
        using error_type = E;

        explicit Future(std::shared_ptr<Core<T, E>> core) : mCore{std::move(core)} {
        }

        Future(Future &&rhs) noexcept : mCore{std::move(rhs.mCore)} {
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
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);
            mCore->callback = std::forward<F>(f);

            auto state = mCore->state.load();

            if (state == State::PENDING && mCore->state.compare_exchange_strong(state, State::ONLY_CALLBACK))
                return;

            if (state != State::ONLY_RESULT || !mCore->state.compare_exchange_strong(state, State::DONE))
                throw std::logic_error(fmt::format("unexpected state: {}", static_cast<int>(state)));

            mCore->trigger();
        }

        template<typename F, std::enable_if_t<detail::is_specialization_v<callback_result_t<F, T>, Future>>* = nullptr>
        auto then(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<typename callback_result_t<F, T>::value_type, E>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                auto next = std::move(result).transform(f);

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                std::move(*next).then([=](auto &&... args) {
                    promise->resolve(std::forward<decltype(args)>(args)...);
                }).fail([=](E &&error) {
                    promise->reject(std::move(error));
                });
            });

            return promise->getFuture();
        }

        template<
            typename F,
            std::enable_if_t<detail::is_specialization_v<callback_result_t<F, T>, tl::expected>>* = nullptr
        >
        auto then(F &&f) && {
            using NextValue = typename callback_result_t<F, T>::value_type;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<NextValue, E>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                auto next = std::move(result).and_then(f);

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                if constexpr (std::is_void_v<NextValue>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(next));
            });

            return promise->getFuture();
        }

        template<
            typename F,
            std::enable_if_t<
                !detail::is_specialization_v<callback_result_t<F, T>, Future> &&
                !detail::is_specialization_v<callback_result_t<F, T>, tl::expected>
            >* = nullptr
        >
        auto then(F &&f) && {
            using NextValue = std::remove_cv_t<std::remove_reference_t<callback_result_t<F, T>>>;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<NextValue, E>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                auto next = std::move(result).transform([&](T &&value) {
                    // the result may be a reference type, so transform cannot be called directly.
                    return std::invoke(f, std::move(value));
                });

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                if constexpr (std::is_void_v<NextValue>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(next));
            });

            return promise->getFuture();
        }

        template<typename F, std::enable_if_t<detail::is_specialization_v<callback_result_t<F, E>, Future>>* = nullptr>
        auto fail(F &&f) && {
            using NextError = typename callback_result_t<F, E>::error_type;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, NextError>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                if (!result) {
                    std::invoke(f, std::move(result).error()).then([=](auto &&... args) {
                        promise->resolve(std::forward<decltype(args)>(args)...);
                    }).fail([=](NextError &&error) {
                        promise->reject(std::move(error));
                    });

                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(result));
            });

            return promise->getFuture();
        }

        template<
            typename F,
            std::enable_if_t<detail::is_specialization_v<callback_result_t<F, E>, tl::expected>>* = nullptr
        >
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, typename callback_result_t<F, E>::error_type>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                auto next = std::move(result).or_else(f);

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(next));
            });

            return promise->getFuture();
        }

        template<
            typename F,
            std::enable_if_t<detail::is_specialization_v<callback_result_t<F, E>, tl::unexpected>>* = nullptr
        >
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<
                Promise<
                    T,
                    std::remove_cv_t<std::remove_reference_t<decltype(std::declval<callback_result_t<F, E>>().value())>>
                >
            >();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                if (!result) {
                    promise->reject(std::invoke(f, std::move(result).error()).value());
                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(result));
            });

            return promise->getFuture();
        }

        template<
            typename F,
            std::enable_if_t<
                !detail::is_specialization_v<callback_result_t<F, E>, Future> &&
                !detail::is_specialization_v<callback_result_t<F, E>, tl::expected> &&
                !detail::is_specialization_v<callback_result_t<F, E>, tl::unexpected>
            >* = nullptr
        >
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, E>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
                if (!result) {
                    if constexpr (std::is_void_v<T>) {
                        std::invoke(f, std::move(result).error());
                        promise->resolve();
                    }
                    else {
                        promise->resolve(std::invoke(f, std::move(result).error()));
                    }

                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(result));
            });

            return promise->getFuture();
        }

        template<typename F>
        auto finally(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, E>>();

            setCallback([=, f = std::forward<F>(f)](tl::expected<T, E> &&result) {
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

    private:
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename T, typename E, typename F>
    Future<T, E> chain(F &&f) {
        Promise<T, E> promise;
        auto future = promise.getFuture();
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
        std::enable_if_t<
            ranges::input_iterator<I> &&
            ranges::sentinel_for<S, I> &&
            detail::is_specialization_v<ranges::iter_value_t<I>, Future>
        >* = nullptr
    >
    auto all(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        if constexpr (std::is_void_v<T>) {
            struct Context {
                explicit Context(const std::size_t n) : count{n} {
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
                explicit Context(const std::size_t n) : count{n}, values(n) {
                }

                Promise<std::vector<T>, E> promise;
                std::atomic<std::size_t> count;
                std::atomic_flag flag = ATOMIC_FLAG_INIT;
                std::vector<T> values;
            };

            const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

            // waiting for libc++ to implement ranges::views::enumerate
            for (std::size_t i{0}; first != last; ++first, ++i) {
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
        std::enable_if_t<
            ranges::input_range<R> &&
            detail::is_specialization_v<ranges::range_value_t<R>, Future>
        >* = nullptr
    >
    auto all(R futures) {
        return all(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename E>
    auto all(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        using T = std::conditional_t<
            detail::all_same_v<Ts...>,
            std::conditional_t<
                std::is_void_v<detail::first_element_t<Ts...>>,
                void,
                std::array<detail::first_element_t<Ts...>, sizeof...(Ts)>
            >,
            std::tuple<std::conditional_t<std::is_void_v<Ts>, std::nullptr_t, Ts>...>
        >;

        if constexpr (std::is_void_v<T>) {
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
                Promise<T, E> promise;
                std::atomic<std::size_t> count{sizeof...(Ts)};
                std::atomic_flag flag = ATOMIC_FLAG_INIT;
                T values;
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
        return all(std::index_sequence_for<Ts...>{}, std::move(futures)...);
    }

    template<
        typename I,
        typename S,
        std::enable_if_t<
            ranges::input_iterator<I> &&
            ranges::sentinel_for<S, I> &&
            detail::is_specialization_v<ranges::iter_value_t<I>, Future>
        >* = nullptr
    >
    auto allSettled(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count{n}, results(n) {
            }

            Promise<std::vector<tl::expected<T, E>>> promise;
            std::atomic<std::size_t> count;
            std::vector<tl::expected<T, E>> results;
        };

        const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

        for (std::size_t i{0}; first != last; ++first, ++i) {
            (*first).setCallback([=](tl::expected<T, E> result) {
                if (!result) {
                    ctx->results[i] = tl::unexpected{std::move(result).error()};

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
        std::enable_if_t<
            ranges::input_range<R> &&
            detail::is_specialization_v<ranges::range_value_t<R>, Future>
        >* = nullptr
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
                    std::get<Is>(ctx->results) = tl::unexpected{std::move(result).error()};

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
        std::enable_if_t<
            ranges::input_iterator<I> &&
            ranges::sentinel_for<S, I> &&
            detail::is_specialization_v<ranges::iter_value_t<I>, Future>
        >* = nullptr
    >
    auto any(I first, S last) {
        using T = typename ranges::iter_value_t<I>::value_type;
        using E = typename ranges::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count{n}, errors(n) {
            }

            Promise<T, std::vector<E>> promise;
            std::atomic<std::size_t> count;
            std::atomic_flag flag = ATOMIC_FLAG_INIT;
            std::vector<E> errors;
        };

        const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(ranges::distance(first, last)));

        for (std::size_t i{0}; first != last; ++first, ++i) {
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
        std::enable_if_t<
            ranges::input_range<R> &&
            detail::is_specialization_v<ranges::range_value_t<R>, Future>
        >* = nullptr
    >
    auto any(R futures) {
        return any(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename E>
    auto any(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        constexpr auto Same = detail::all_same_v<Ts...>;

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
        std::enable_if_t<
            ranges::input_iterator<I> &&
            ranges::sentinel_for<S, I> &&
            detail::is_specialization_v<ranges::iter_value_t<I>, Future>
        >* = nullptr
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
        std::enable_if_t<
            ranges::input_range<R> &&
            detail::is_specialization_v<ranges::range_value_t<R>, Future>
        >* = nullptr
    >
    auto race(R futures) {
        return race(futures.begin(), futures.end());
    }

    template<typename... Ts, typename E>
    auto race(Future<Ts, E>... futures) {
        constexpr auto Same = detail::all_same_v<Ts...>;

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
