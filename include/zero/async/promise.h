#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <any>
#include <mutex>
#include <memory>
#include <ranges>
#include <cassert>
#include <utility>
#include <optional>
#include <expected>
#include <stdexcept>
#include <functional>
#include <fmt/core.h>
#include <zero/atomic/event.h>
#include <zero/detail/type_traits.h>

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
        std::optional<std::expected<T, E>> result;
        std::function<void(std::expected<T, E>)> callback;

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
                throw std::logic_error{"future already retrieved"};

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
                mCore->result.emplace(std::in_place, std::forward<Ts>(args)...);
            }

            auto state = mCore->state.load();

            if (state == State::PENDING && mCore->state.compare_exchange_strong(state, State::ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != State::ONLY_CALLBACK || !mCore->state.compare_exchange_strong(state, State::DONE))
                throw std::logic_error{fmt::format("unexpected state: {}", std::to_underlying(state))};

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

            mCore->result.emplace(std::unexpected<E>(std::in_place, std::forward<Ts>(args)...));
            auto state = mCore->state.load();

            if (state == State::PENDING && mCore->state.compare_exchange_strong(state, State::ONLY_RESULT)) {
                mCore->event.set();
                return;
            }

            if (state != State::ONLY_CALLBACK || !mCore->state.compare_exchange_strong(state, State::DONE))
                throw std::logic_error{fmt::format("unexpected state: {}", std::to_underlying(state))};

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

        std::expected<T, E> &result() & {
            assert(mCore);
            assert(mCore->result);
            return *mCore->result;
        }

        [[nodiscard]] const std::expected<T, E> &result() const & {
            assert(mCore);
            assert(mCore->result);
            return *mCore->result;
        }

        std::expected<T, E> &&result() && {
            assert(mCore);
            assert(mCore->result);
            return std::move(*mCore->result);
        }

        [[nodiscard]] const std::expected<T, E> &&result() const && {
            assert(mCore);
            assert(mCore->result);
            return std::move(*mCore->result);
        }

        [[nodiscard]] std::expected<void, std::error_code>
        wait(const std::optional<std::chrono::milliseconds> timeout = std::nullopt) const {
            assert(mCore);

            if (mCore->hasResult())
                return {};

            return mCore->event.wait(timeout);
        }

        std::expected<T, E> get() && {
            if (const auto result = wait(); !result)
                throw std::system_error{result.error()};

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
                throw std::logic_error{fmt::format("unexpected state: {}", std::to_underlying(state))};

            mCore->trigger();
        }

        template<typename F>
            requires detail::is_specialization_v<callback_result_t<F, T>, Future>
        auto then(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<typename callback_result_t<F, T>::value_type, E>>();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
                auto next = std::move(result).transform(f);

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                std::move(*next).then([=]<typename... Ts>(Ts &&... args) {
                    promise->resolve(std::forward<Ts>(args)...);
                }).fail([=](E &&error) {
                    promise->reject(std::move(error));
                });
            });

            return promise->getFuture();
        }

        template<typename F>
            requires detail::is_specialization_v<callback_result_t<F, T>, std::expected>
        auto then(F &&f) && {
            using NextValue = typename callback_result_t<F, T>::value_type;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<NextValue, E>>();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
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

        template<typename F>
            requires (
                !detail::is_specialization_v<callback_result_t<F, T>, Future> &&
                !detail::is_specialization_v<callback_result_t<F, T>, std::expected>
            )
        auto then(F &&f) && {
            using NextValue = callback_result_t<F, T>;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<NextValue, E>>();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
                auto next = std::move(result).transform(f);

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

        template<typename F>
            requires detail::is_specialization_v<callback_result_t<F, E>, Future>
        auto fail(F &&f) && {
            using NextError = typename callback_result_t<F, E>::error_type;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, NextError>>();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
                if (!result) {
                    std::invoke(f, std::move(result).error()).then([=]<typename... Ts>(Ts &&... args) {
                        promise->resolve(std::forward<Ts>(args)...);
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

        template<typename F>
            requires detail::is_specialization_v<callback_result_t<F, E>, std::expected>
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, typename callback_result_t<F, E>::error_type>>();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
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

        template<typename F>
            requires detail::is_specialization_v<callback_result_t<F, E>, std::unexpected>
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<
                Promise<T, std::remove_cvref_t<decltype(std::declval<callback_result_t<F, E>>().error())>>
            >();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
                if (!result) {
                    promise->reject(std::invoke(f, std::move(result).error()).error());
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
            requires (
                !detail::is_specialization_v<callback_result_t<F, E>, Future> &&
                !detail::is_specialization_v<callback_result_t<F, E>, std::expected> &&
                !detail::is_specialization_v<callback_result_t<F, E>, std::unexpected>
            )
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::ONLY_CALLBACK);
            assert(mCore->state != State::DONE);

            const auto promise = std::make_shared<Promise<T, E>>();

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
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

            setCallback([=, f = std::forward<F>(f)](std::expected<T, E> &&result) {
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

    template<typename T>
    struct optional_wrapper;

    template<typename T>
    struct optional_wrapper<std::vector<T>> {
        using type = std::vector<std::optional<T>>;
    };

    template<typename T, std::size_t N>
    struct optional_wrapper<std::array<T, N>> {
        using type = std::array<std::optional<T>, N>;
    };

    template<typename... Ts>
    struct optional_wrapper<std::tuple<Ts...>> {
        using type = std::tuple<std::optional<Ts>...>;
    };

    template<typename T>
    using optional_wrapper_t = typename optional_wrapper<T>::type;

    template<typename T>
    std::vector<T> unwrap(std::vector<std::optional<T>> &&vector) {
        return vector
            | std::views::as_rvalue
            | std::views::transform([](std::optional<T> &&value) {
                assert(value);
                return *std::move(value);
            })
            | std::ranges::to<std::vector>();
    }

    template<std::size_t... Is, typename T, std::size_t N>
    std::array<T, N> unwrap(std::index_sequence<Is...>, std::array<std::optional<T>, N> &&array) {
        assert((std::get<Is>(array) && ...));
        return {*std::move(std::get<Is>(array))...};
    }

    template<typename T, std::size_t N>
    std::array<T, N> unwrap(std::array<std::optional<T>, N> &&array) {
        return unwrap(std::make_index_sequence<N>{}, std::move(array));
    }

    template<std::size_t... Is, typename... Ts>
    std::tuple<Ts...> unwrap(std::index_sequence<Is...>, std::tuple<std::optional<Ts>...> &&tuple) {
        assert((std::get<Is>(tuple) && ...));
        return {*std::move(std::get<Is>(tuple))...};
    }

    template<typename... Ts>
    std::tuple<Ts...> unwrap(std::tuple<std::optional<Ts>...> &&tuple) {
        return unwrap(std::index_sequence_for<Ts...>{}, std::move(tuple));
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization_v<std::iter_value_t<I>, Future>
    auto all(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        if constexpr (std::is_void_v<T>) {
            struct Context {
                explicit Context(const std::size_t n) : count{n} {
                }

                Promise<void, E> promise;
                std::atomic<std::size_t> count;
                std::atomic_flag flag;
            };

            const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(std::ranges::distance(first, last)));

            while (first != last) {
                (*first++).setCallback([=](std::expected<T, E> &&result) {
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
                std::atomic_flag flag;
                optional_wrapper_t<std::vector<T>> values;
            };

            const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(std::ranges::distance(first, last)));

            // waiting for libc++ to implement std::ranges::views::enumerate
            for (std::size_t i{0}; first != last; ++first, ++i) {
                (*first).setCallback([=](std::expected<T, E> &&result) {
                    if (!result) {
                        if (!ctx->flag.test_and_set())
                            ctx->promise.reject(std::move(result).error());

                        return;
                    }

                    ctx->values[i] = *std::move(result);

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->values)));
                });
            }

            return ctx->promise.getFuture();
        }
    }

    template<std::ranges::input_range R>
        requires detail::is_specialization_v<std::ranges::range_value_t<R>, Future>
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
                std::atomic_flag flag;
            };

            const auto ctx = std::make_shared<Context>();

            ([&] {
                futures.setCallback([=](std::expected<Ts, E> result) {
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
                std::atomic_flag flag;
                optional_wrapper_t<T> values;
            };

            const auto ctx = std::make_shared<Context>();

            ([&] {
                futures.setCallback([=](std::expected<Ts, E> result) {
                    if (!result) {
                        if (!ctx->flag.test_and_set())
                            ctx->promise.reject(std::move(result).error());

                        return;
                    }

                    if constexpr (std::is_void_v<Ts>) {
                        std::get<Is>(ctx->values).emplace();

                        if (--ctx->count > 0)
                            return;

                        ctx->promise.resolve(unwrap(std::move(ctx->values)));
                    }
                    else {
                        std::get<Is>(ctx->values) = *std::move(result);

                        if (--ctx->count > 0)
                            return;

                        ctx->promise.resolve(unwrap(std::move(ctx->values)));
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

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization_v<std::iter_value_t<I>, Future>
    auto allSettled(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count{n}, results(n) {
            }

            Promise<std::vector<std::expected<T, E>>> promise;
            std::atomic<std::size_t> count;
            optional_wrapper_t<std::vector<std::expected<T, E>>> results;
        };

        const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(std::ranges::distance(first, last)));

        for (std::size_t i{0}; first != last; ++first, ++i) {
            (*first).setCallback([=](std::expected<T, E> &&result) {
                if (!result) {
                    ctx->results[i] = std::unexpected{std::move(result).error()};

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->results)));
                    return;
                }

                if constexpr (std::is_void_v<T>) {
                    ctx->results[i].emplace();

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->results)));
                }
                else {
                    ctx->results[i] = *std::move(result);

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->results)));
                }
            });
        }

        return ctx->promise.getFuture();
    }

    template<std::ranges::input_range R>
        requires detail::is_specialization_v<std::ranges::range_value_t<R>, Future>
    auto allSettled(R futures) {
        return allSettled(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename... Es>
    auto allSettled(std::index_sequence<Is...>, Future<Ts, Es>... futures) {
        using T = std::conditional_t<
            detail::all_same_v<Ts...>,
            std::array<std::expected<detail::first_element_t<Ts...>, detail::first_element_t<Es...>>, sizeof...(Ts)>,
            std::tuple<std::expected<Ts, Es>...>
        >;

        struct Context {
            Promise<T> promise;
            std::atomic<std::size_t> count{sizeof...(Ts)};
            optional_wrapper_t<T> results;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](std::expected<Ts, Es> result) {
                if (!result) {
                    std::get<Is>(ctx->results) = std::unexpected{std::move(result).error()};

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->results)));
                    return;
                }

                if constexpr (std::is_void_v<Ts>) {
                    std::get<Is>(ctx->results).emplace();

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->results)));
                }
                else {
                    std::get<Is>(ctx->results) = *std::move(result);

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.resolve(unwrap(std::move(ctx->results)));
                }
            });
        }(), ...);

        return ctx->promise.getFuture();
    }

    template<typename... Ts, typename... Es>
    auto allSettled(Future<Ts, Es>... promises) {
        return allSettled(std::index_sequence_for<Ts...>{}, std::move(promises)...);
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization_v<std::iter_value_t<I>, Future>
    auto any(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count{n}, errors(n) {
            }

            Promise<T, std::vector<E>> promise;
            std::atomic<std::size_t> count;
            std::atomic_flag flag;
            optional_wrapper_t<std::vector<E>> errors;
        };

        const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(std::ranges::distance(first, last)));

        for (std::size_t i{0}; first != last; ++first, ++i) {
            (*first).setCallback([=](std::expected<T, E> &&result) {
                if (!result) {
                    ctx->errors[i] = std::move(result).error();

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.reject(unwrap(std::move(ctx->errors)));
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

    template<std::ranges::input_range R>
        requires detail::is_specialization_v<std::ranges::range_value_t<R>, Future>
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
            std::atomic_flag flag;
            optional_wrapper_t<std::array<E, sizeof...(Ts)>> errors;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](std::expected<Ts, E> result) {
                if (!result) {
                    std::get<Is>(ctx->errors) = std::move(result).error();

                    if (--ctx->count > 0)
                        return;

                    ctx->promise.reject(unwrap(std::move(ctx->errors)));
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

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization_v<std::iter_value_t<I>, Future>
    auto race(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        struct Context {
            Promise<T, E> promise;
            std::atomic_flag flag;
        };

        const auto ctx = std::make_shared<Context>();

        while (first != last) {
            (*first++).setCallback([=](std::expected<T, E> &&result) {
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

    template<std::ranges::input_range R>
        requires detail::is_specialization_v<std::ranges::range_value_t<R>, Future>
    auto race(R futures) {
        return race(futures.begin(), futures.end());
    }

    template<typename... Ts, typename E>
    auto race(Future<Ts, E>... futures) {
        constexpr auto Same = detail::all_same_v<Ts...>;

        struct Context {
            Promise<std::conditional_t<Same, detail::first_element_t<Ts...>, std::any>, E> promise;
            std::atomic_flag flag;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](std::expected<Ts, E> result) {
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
