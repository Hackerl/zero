#ifndef ZERO_ASYNC_PROMISE_H
#define ZERO_ASYNC_PROMISE_H

#include <any>
#include <memory>
#include <ranges>
#include <cassert>
#include <utility>
#include <optional>
#include <stdexcept>
#include <fmt/core.h>
#include <zero/error.h>
#include <zero/atomic/event.h>
#include <zero/meta/concepts.h>

namespace zero::async::promise {
    enum class State {
        Pending,
        OnlyCallback,
        OnlyResult,
        Done
    };

    template<typename T, typename E>
    struct Core {
        atomic::Event event{true};
        std::atomic<State> state{State::Pending};
        std::optional<std::expected<T, E>> result;
        std::function<void(std::expected<T, E>)> callback;

        void trigger() {
            assert(state == State::Done);
            std::exchange(callback, nullptr)(std::move(*result));
        }

        [[nodiscard]] bool hasResult() const {
            const auto s = state.load();
            return s == State::OnlyResult || s == State::Done;
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

            if (mRetrieved)
                throw error::StacktraceError<std::logic_error>{"Future already retrieved"};

            mRetrieved = true;
            return Future<T, E>{mCore};
        }

        template<typename... Args>
        void resolve(Args &&... args) {
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->state != State::OnlyResult);
            assert(mCore->state != State::Done);

            if constexpr (std::is_void_v<T>) {
                static_assert(sizeof...(Args) == 0);
                mCore->result.emplace();
            }
            else {
                static_assert(sizeof...(Args) > 0);
                mCore->result.emplace(std::in_place, std::forward<Args>(args)...);
            }

            auto state = mCore->state.load();

            if (state == State::Pending && mCore->state.compare_exchange_strong(state, State::OnlyResult)) {
                mCore->event.set();
                return;
            }

            if (state != State::OnlyCallback || !mCore->state.compare_exchange_strong(state, State::Done))
                throw error::StacktraceError<std::logic_error>{
                    fmt::format("Unexpected promise state: {}", std::to_underlying(state))
                };

            mCore->event.set();
            mCore->trigger();
        }

        template<typename... Args>
        void reject(Args &&... args) {
            static_assert(sizeof...(Args) > 0);
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->state != State::OnlyResult);
            assert(mCore->state != State::Done);

            mCore->result.emplace(std::unexpected<E>(std::in_place, std::forward<Args>(args)...));
            auto state = mCore->state.load();

            if (state == State::Pending && mCore->state.compare_exchange_strong(state, State::OnlyResult)) {
                mCore->event.set();
                return;
            }

            if (state != State::OnlyCallback || !mCore->state.compare_exchange_strong(state, State::Done))
                throw error::StacktraceError<std::logic_error>{
                    fmt::format("Unexpected promise state: {}", std::to_underlying(state))
                };

            mCore->event.set();
            mCore->trigger();
        }

    protected:
        bool mRetrieved;
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename T, typename E>
    Future<T, E> chain(std::function<void(Promise<T, E>)> f) {
        Promise<T, E> promise;
        auto future = promise.getFuture();
        f(std::move(promise));
        return future;
    }

    template<typename T, typename E, typename... Args>
    Future<T, E> reject(Args &&... args) {
        Promise<T, E> promise;
        promise.reject(std::forward<Args>(args)...);
        return promise.getFuture();
    }

    template<typename T, typename E>
        requires std::is_void_v<T>
    Future<T, E> resolve() {
        Promise<T, E> promise;
        promise.resolve();
        return promise.getFuture();
    }

    template<typename T, typename E, typename... Args>
        requires (!std::is_void_v<T> && sizeof...(Args) > 0)
    Future<T, E> resolve(Args &&... args) {
        Promise<T, E> promise;
        promise.resolve(std::forward<Args>(args)...);
        return promise.getFuture();
    }

    template<typename F, typename T>
    using CallbackResult = std::conditional_t<
        std::is_void_v<T>,
        std::invoke_result<F>,
        std::invoke_result<F, T>
    >::type;

    template<typename F, typename T>
    concept AsyncCallback =
        (!std::is_void_v<T> && requires(F &&f, T &&arg) {
            { std::invoke(std::forward<F>(f), std::forward<T>(arg)) } -> meta::Specialization<Future>;
        }) ||
        (std::is_void_v<T> && requires(F &&f) {
            { std::invoke(std::forward<F>(f)) } -> meta::Specialization<Future>;
        });

    template<typename F, typename T>
    concept FallibleCallback =
        (!std::is_void_v<T> && requires(F &&f, T &&arg) {
            { std::invoke(std::forward<F>(f), std::forward<T>(arg)) } -> meta::Specialization<std::expected>;
        }) ||
        (std::is_void_v<T> && requires(F &&f) {
            { std::invoke(std::forward<F>(f)) } -> meta::Specialization<std::expected>;
        });

    template<typename F, typename T>
    concept FailingCallback =
        (!std::is_void_v<T> && requires(F &&f, T &&arg) {
            { std::invoke(std::forward<F>(f), std::forward<T>(arg)) } -> meta::Specialization<std::unexpected>;
        }) ||
        (std::is_void_v<T> && requires(F &&f) {
            { std::invoke(std::forward<F>(f)) } -> meta::Specialization<std::unexpected>;
        });

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
            error::guard(wait());
            return std::move(*mCore->result);
        }

        void setCallback(std::function<void(std::expected<T, E>)> callback) {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);
            mCore->callback = std::move(callback);

            auto state = mCore->state.load();

            if (state == State::Pending && mCore->state.compare_exchange_strong(state, State::OnlyCallback))
                return;

            if (state != State::OnlyResult || !mCore->state.compare_exchange_strong(state, State::Done))
                throw error::StacktraceError<std::logic_error>{
                    fmt::format("Unexpected promise state: {}", std::to_underlying(state))
                };

            mCore->trigger();
        }

        template<AsyncCallback<T> F>
        auto then(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<typename CallbackResult<F, T>::value_type, E>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                auto next = std::move(result).transform(std::move(f));

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                std::move(*next).then([=]<typename... Args>(Args &&... args) {
                    promise->resolve(std::forward<Args>(args)...);
                }).fail([=](E &&error) {
                    promise->reject(std::move(error));
                });
            });

            return future;
        }

        template<FallibleCallback<T> F>
        auto then(F &&f) && {
            using NextValue = CallbackResult<F, T>::value_type;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<NextValue, E>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                auto next = std::move(result).and_then(std::move(f));

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                if constexpr (std::is_void_v<NextValue>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(next));
            });

            return future;
        }

        template<typename F>
            requires (!AsyncCallback<F, T> && !FallibleCallback<F, T>)
        auto then(F &&f) && {
            using NextValue = CallbackResult<F, T>;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<NextValue, E>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                auto next = std::move(result).transform(std::move(f));

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                if constexpr (std::is_void_v<NextValue>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(next));
            });

            return future;
        }

        template<typename F1, typename F2>
            requires requires(Future self, F1 &&f1, F2 &&f2) {
                requires requires(decltype(std::move(self).then(std::forward<F1>(f1))) next) {
                    { std::move(next).fail(std::forward<F2>(f2)) } -> std::same_as<decltype(next)>;
                };
            }
        auto then(F1 &&f1, F2 &&f2) && {
            const auto fallthrough = std::make_shared<bool>();

            auto future = std::move(*this)
                .then([=, f = std::forward<F1>(f1)]<typename... Args>(Args &&... args) mutable {
                    *fallthrough = true;
                    return std::invoke(std::move(f), std::forward<Args>(args)...);
                });

            using NextValue = decltype(future)::value_type;

            return std::move(future).fail([=, f = std::forward<F2>(f2)](E &&error) mutable {
                if (*fallthrough)
                    return reject<NextValue, E>(std::move(error));

                return reject<NextValue, E>(std::move(error)).fail(std::move(f));
            });
        }

        template<AsyncCallback<E> F>
        auto fail(F &&f) && {
            using NextError = CallbackResult<F, E>::error_type;

            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<T, NextError>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                if (!result) {
                    std::invoke(std::move(f), std::move(result).error()).then([=]<typename... Args>(Args &&... args) {
                        promise->resolve(std::forward<Args>(args)...);
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

            return future;
        }

        template<FallibleCallback<E> F>
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<T, typename CallbackResult<F, E>::error_type>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                auto next = std::move(result).or_else(std::move(f));

                if (!next) {
                    promise->reject(std::move(next).error());
                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(next));
            });

            return future;
        }

        template<FailingCallback<E> F>
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<
                Promise<T, std::remove_cvref_t<decltype(std::declval<CallbackResult<F, E>>().error())>>
            >();

            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                if (!result) {
                    promise->reject(std::invoke(std::move(f), std::move(result).error()).error());
                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(result));
            });

            return future;
        }

        template<typename F>
            requires (!AsyncCallback<F, E> && !FallibleCallback<F, E> && !FailingCallback<F, E>)
        auto fail(F &&f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<T, E>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::forward<F>(f)](std::expected<T, E> &&result) mutable {
                if (!result) {
                    if constexpr (std::is_void_v<T>) {
                        std::invoke(std::move(f), std::move(result).error());
                        promise->resolve();
                    }
                    else {
                        promise->resolve(std::invoke(std::move(f), std::move(result).error()));
                    }

                    return;
                }

                if constexpr (std::is_void_v<T>)
                    promise->resolve();
                else
                    promise->resolve(*std::move(result));
            });

            return future;
        }

        auto finally(std::function<void()> f) && {
            assert(mCore);
            assert(!mCore->callback);
            assert(mCore->state != State::OnlyCallback);
            assert(mCore->state != State::Done);

            const auto promise = std::make_shared<Promise<T, E>>();
            auto future = promise->getFuture();

            setCallback([promise = std::move(promise), f = std::move(f)](std::expected<T, E> &&result) mutable {
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

            return future;
        }

    private:
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename T>
    struct OptionalWrapperImpl;

    template<typename T>
    struct OptionalWrapperImpl<std::vector<T>> {
        using Type = std::vector<std::optional<T>>;
    };

    template<typename T, std::size_t N>
    struct OptionalWrapperImpl<std::array<T, N>> {
        using Type = std::array<std::optional<T>, N>;
    };

    template<typename... Ts>
    struct OptionalWrapperImpl<std::tuple<Ts...>> {
        using Type = std::tuple<std::optional<Ts>...>;
    };

    template<typename T>
    using OptionalWrapper = OptionalWrapperImpl<T>::Type;

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
        requires meta::Specialization<std::iter_value_t<I>, Future>
    auto all(I first, S last) {
        if (first == last)
            throw error::StacktraceError<std::invalid_argument>{"Range must not be empty"};

        using T = std::iter_value_t<I>::value_type;
        using E = std::iter_value_t<I>::error_type;

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
                OptionalWrapper<std::vector<T>> values;
            };

            const auto ctx = std::make_shared<Context>(static_cast<std::size_t>(std::ranges::distance(first, last)));

            // Waiting for libc++ to implement std::ranges::views::enumerate
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
        requires meta::Specialization<std::ranges::range_value_t<R>, Future>
    auto all(R futures) {
        return all(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename E>
    auto all(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        static_assert(sizeof...(Ts) > 0);

        using T = std::conditional_t<
            meta::IsAllSame<Ts...>,
            std::conditional_t<
                std::is_void_v<meta::FirstElement<Ts...>>,
                void,
                std::array<meta::FirstElement<Ts...>, sizeof...(Ts)>
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
                futures.setCallback([=](std::expected<Ts, E> &&result) {
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
                OptionalWrapper<T> values;
            };

            const auto ctx = std::make_shared<Context>();

            ([&] {
                futures.setCallback([=](std::expected<Ts, E> &&result) {
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
        requires meta::Specialization<std::iter_value_t<I>, Future>
    auto allSettled(I first, S last) {
        if (first == last)
            throw error::StacktraceError<std::invalid_argument>{"Range must not be empty"};

        using T = std::iter_value_t<I>::value_type;
        using E = std::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count{n}, results(n) {
            }

            Promise<std::vector<std::expected<T, E>>> promise;
            std::atomic<std::size_t> count;
            OptionalWrapper<std::vector<std::expected<T, E>>> results;
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
        requires meta::Specialization<std::ranges::range_value_t<R>, Future>
    auto allSettled(R futures) {
        return allSettled(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename... Es>
    auto allSettled(std::index_sequence<Is...>, Future<Ts, Es>... futures) {
        static_assert(sizeof...(Ts) > 0);

        using T = std::conditional_t<
            meta::IsAllSame<Ts...>,
            std::array<std::expected<meta::FirstElement<Ts...>, meta::FirstElement<Es...>>, sizeof...(Ts)>,
            std::tuple<std::expected<Ts, Es>...>
        >;

        struct Context {
            Promise<T> promise;
            std::atomic<std::size_t> count{sizeof...(Ts)};
            OptionalWrapper<T> results;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](std::expected<Ts, Es> &&result) {
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
        requires meta::Specialization<std::iter_value_t<I>, Future>
    auto any(I first, S last) {
        if (first == last)
            throw error::StacktraceError<std::invalid_argument>{"Range must not be empty"};

        using T = std::iter_value_t<I>::value_type;
        using E = std::iter_value_t<I>::error_type;

        struct Context {
            explicit Context(const std::size_t n) : count{n}, errors(n) {
            }

            Promise<T, std::vector<E>> promise;
            std::atomic<std::size_t> count;
            std::atomic_flag flag;
            OptionalWrapper<std::vector<E>> errors;
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
        requires meta::Specialization<std::ranges::range_value_t<R>, Future>
    auto any(R futures) {
        return any(futures.begin(), futures.end());
    }

    template<std::size_t... Is, typename... Ts, typename E>
    auto any(std::index_sequence<Is...>, Future<Ts, E>... futures) {
        static_assert(sizeof...(Ts) > 0);

        constexpr auto Same = meta::IsAllSame<Ts...>;

        struct Context {
            Promise<
                std::conditional_t<
                    Same,
                    meta::FirstElement<Ts...>,
                    std::any
                >,
                std::array<E, sizeof...(Ts)>
            >
            promise;
            std::atomic<std::size_t> count{sizeof...(Ts)};
            std::atomic_flag flag;
            OptionalWrapper<std::array<E, sizeof...(Ts)>> errors;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](std::expected<Ts, E> &&result) {
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
        requires meta::Specialization<std::iter_value_t<I>, Future>
    auto race(I first, S last) {
        if (first == last)
            throw error::StacktraceError<std::invalid_argument>{"Range must not be empty"};

        using T = std::iter_value_t<I>::value_type;
        using E = std::iter_value_t<I>::error_type;

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
        requires meta::Specialization<std::ranges::range_value_t<R>, Future>
    auto race(R futures) {
        return race(futures.begin(), futures.end());
    }

    template<typename... Ts, typename E>
    auto race(Future<Ts, E>... futures) {
        static_assert(sizeof...(Ts) > 0);

        constexpr auto Same = meta::IsAllSame<Ts...>;

        struct Context {
            Promise<std::conditional_t<Same, meta::FirstElement<Ts...>, std::any>, E> promise;
            std::atomic_flag flag;
        };

        const auto ctx = std::make_shared<Context>();

        ([&] {
            futures.setCallback([=](std::expected<Ts, E> &&result) {
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

#endif //ZERO_ASYNC_PROMISE_H
