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

    class IExecutor {
    public:
        virtual ~IExecutor() = default;
        virtual void post(std::function<void()> f) = 0;
    };

    class InlineExecutor : public IExecutor {
    public:
        static std::shared_ptr<InlineExecutor> instance();

        void post(std::function<void()> f) override;
    };

    template<typename T, typename E>
    struct Core {
        atomic::Event event{true};
        std::atomic<State> state{State::Pending};
        std::optional<std::expected<T, E>> result;
        std::function<void(std::expected<T, E>)> callback;
        std::shared_ptr<IExecutor> executor;

        void trigger() {
            assert(state == State::Done);
            assert(executor);

            executor->post(
                [
                    callback = std::move(callback),
                    result = std::make_shared<std::expected<T, E>>(*std::exchange(result, std::nullopt))
                ] {
                    callback(std::move(*result));
                }
            );
        }

        [[nodiscard]] bool hasResult() const {
            const auto s = state.load();
            return s == State::OnlyResult || s == State::Done;
        }
    };

    template<typename T, typename E = std::exception_ptr>
    class SemiFuture;

    template<typename T, typename E = std::exception_ptr>
    class Future;

    template<typename T, typename E = std::exception_ptr>
    class Promise {
    public:
        using value_type = T;
        using error_type = E;

        Promise() : mRetrieved{false}, mCore{std::make_shared<Core<T, E>>()} {
        }

        explicit Promise(std::shared_ptr<Core<T, E>> core) : mRetrieved{false}, mCore{std::move(core)} {
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

        SemiFuture<T, E> getFuture() {
            assert(mCore);

            if (mRetrieved)
                throw error::StacktraceError<std::logic_error>{"Future already retrieved"};

            mRetrieved = true;
            return SemiFuture<T, E>{mCore};
        }

        void resolve() requires std::is_void_v<T> {
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->state != State::OnlyResult);
            assert(mCore->state != State::Done);

            mCore->result.emplace();
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

        template<typename U = T>
            requires std::constructible_from<T, U>
        void resolve(U &&value) requires (!std::is_void_v<T>) {
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->state != State::OnlyResult);
            assert(mCore->state != State::Done);

            mCore->result.emplace(std::in_place, std::forward<U>(value));
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

        template<typename U>
            requires std::constructible_from<E, U>
        void reject(U &&error) {
            assert(mCore);
            assert(!mCore->result);
            assert(mCore->state != State::OnlyResult);
            assert(mCore->state != State::Done);

            mCore->result.emplace(std::unexpect, std::forward<U>(error));
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

        void reject(const std::derived_from<std::exception> auto &exception)
            requires std::same_as<E, std::exception_ptr> {
            reject(std::make_exception_ptr(exception));
        }

    protected:
        bool mRetrieved;
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename T, typename E = std::exception_ptr>
    using Contract = std::pair<Promise<T, E>, Future<T, E>>;

    template<typename T, typename E = std::exception_ptr>
    Contract<T, E> makeContract(std::shared_ptr<IExecutor> executor) {
        Promise<T, E> promise;
        auto future = promise.getFuture().via(std::move(executor));
        return {std::move(promise), std::move(future)};
    }

    template<typename T>
    struct ResolvedFuture {
        T value;

        // ReSharper disable once CppNonExplicitConversionOperator
        template<typename E>
        operator SemiFuture<T, E>() && {
            return SemiFuture<T, E>::resolved(std::move(value));
        }

        // ReSharper disable once CppNonExplicitConversionOperator
        template<typename E>
        operator Future<T, E>() && {
            return Future<T, E>::resolved(std::move(value));
        }
    };

    template<>
    struct ResolvedFuture<void> {
        // ReSharper disable once CppNonExplicitConversionOperator
        template<typename E>
        operator SemiFuture<void, E>() && {
            return SemiFuture<void, E>::resolved();
        }

        // ReSharper disable once CppNonExplicitConversionOperator
        template<typename E>
        operator Future<void, E>() && {
            return Future<void, E>::resolved();
        }
    };

    template<typename E>
    struct RejectedFuture {
        E error;

        // ReSharper disable once CppNonExplicitConversionOperator
        template<typename T>
        operator SemiFuture<T, E>() && {
            return SemiFuture<T, E>::rejected(std::move(error));
        }

        // ReSharper disable once CppNonExplicitConversionOperator
        template<typename T>
        operator Future<T, E>() && {
            return Future<T, E>::rejected(std::move(error));
        }
    };

    template<typename E>
    RejectedFuture<E> reject(E error) {
        return {std::move(error)};
    }

    inline ResolvedFuture<void> resolve() {
        return {};
    }

    template<typename T>
    ResolvedFuture<T> resolve(T value) {
        return {std::move(value)};
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

    template<typename F, typename T>
    concept Callback =
        (!std::is_void_v<T> && requires(F &&f, T &&arg) {
            { std::invoke(std::forward<F>(f), std::forward<T>(arg)) };
        }) ||
        (std::is_void_v<T> && requires(F &&f) {
            { std::invoke(std::forward<F>(f)) };
        });

    template<typename T, typename E>
    class FutureBase {
    public:
        using value_type = T;
        using error_type = E;

        explicit FutureBase(std::shared_ptr<Core<T, E>> core) : mCore{std::move(core)} {
        }

        FutureBase(FutureBase &&rhs) noexcept : mCore{std::move(rhs.mCore)} {
        }

        FutureBase &operator=(FutureBase &&rhs) noexcept {
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

        T get() && requires std::same_as<E, std::exception_ptr> {
            error::guard(wait());

            if (!*mCore->result)
                std::rethrow_exception(mCore->result->error());

            if constexpr (std::is_void_v<T>)
                return;
            else
                return *std::move(*mCore->result);
        }

        std::expected<T, E> get() && requires (!std::same_as<E, std::exception_ptr>) {
            error::guard(wait());
            return std::move(*mCore->result);
        }

    protected:
        std::shared_ptr<Core<T, E>> mCore;
    };

    template<typename T, typename E>
    class SemiFuture final : public FutureBase<T, E> {
    public:
        using FutureBase<T, E>::FutureBase;

        static SemiFuture resolved() requires std::is_void_v<T> {
            Promise<T, E> promise;
            promise.resolve();
            return promise.getFuture();
        }

        template<typename U = T>
            requires std::constructible_from<T, U>
        static SemiFuture resolved(U value) requires (!std::is_void_v<T>) {
            Promise<T, E> promise;
            promise.resolve(std::move(value));
            return promise.getFuture();
        }

        static SemiFuture rejected(E error) {
            Promise<T, E> promise;
            promise.reject(std::move(error));
            return promise.getFuture();
        }

        Future<T, E> via(std::shared_ptr<IExecutor> executor = InlineExecutor::instance()) && {
            this->mCore->executor = std::move(executor);
            return Future<T, E>{std::move(this->mCore)};
        }
    };

    template<typename T, typename E>
    class Future final : public FutureBase<T, E> {
    public:
        using FutureBase<T, E>::FutureBase;

        static Future resolved() requires std::is_void_v<T> {
            return SemiFuture<T, E>::resolved().via();
        }

        template<typename U = T>
            requires std::constructible_from<T, U>
        static Future resolved(U value) requires (!std::is_void_v<T>) {
            return SemiFuture<T, E>::resolved(std::move(value)).via();
        }

        static Future rejected(E error) {
            return SemiFuture<T, E>::rejected(std::move(error)).via();
        }

        Future &&via(std::shared_ptr<IExecutor> executor) && {
            this->mCore->executor = std::move(executor);
            return std::move(*this);
        }

        void setCallback(std::function<void(std::expected<T, E>)> callback) {
            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);
            this->mCore->callback = std::move(callback);

            auto state = this->mCore->state.load();

            if (state == State::Pending && this->mCore->state.compare_exchange_strong(state, State::OnlyCallback))
                return;

            if (state != State::OnlyResult || !this->mCore->state.compare_exchange_strong(state, State::Done))
                throw error::StacktraceError<std::logic_error>{
                    fmt::format("Unexpected promise state: {}", std::to_underlying(state))
                };

            this->mCore->trigger();
        }

        template<AsyncCallback<T> F>
        auto then(F &&f) && {
            static_assert(std::is_same_v<typename CallbackResult<F, T>::error_type, E>);
            using NextValue = CallbackResult<F, T>::value_type;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<NextValue, E>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<NextValue, E>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
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
                }
            );

            return std::move(future);
        }

        template<Callback<T> F>
            requires (!AsyncCallback<F, T> && !FallibleCallback<F, T>)
        auto then(F &&f) && requires std::same_as<E, std::exception_ptr> {
            using NextValue = CallbackResult<F, T>;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<NextValue>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<NextValue>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    try {
                        auto next = std::move(result).transform(std::move(f));

                        if (!next) {
                            promise->reject(std::move(next).error());
                            return;
                        }

                        if constexpr (std::is_void_v<NextValue>)
                            promise->resolve();
                        else
                            promise->resolve(*std::move(next));
                    }
                    catch (const std::exception &) {
                        promise->reject(std::current_exception());
                    }
                }
            );

            return std::move(future);
        }

        template<FallibleCallback<T> F>
        auto then(F &&f) && requires (!std::same_as<E, std::exception_ptr>) {
            static_assert(std::is_same_v<typename CallbackResult<F, T>::error_type, E>);
            using NextValue = CallbackResult<F, T>::value_type;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<NextValue, E>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<NextValue, E>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    auto next = std::move(result).and_then(std::move(f));

                    if (!next) {
                        promise->reject(std::move(next).error());
                        return;
                    }

                    if constexpr (std::is_void_v<NextValue>)
                        promise->resolve();
                    else
                        promise->resolve(*std::move(next));
                }
            );

            return std::move(future);
        }

        template<Callback<T> F>
            requires (!AsyncCallback<F, T> && !FallibleCallback<F, T>)
        auto then(F &&f) && requires (!std::same_as<E, std::exception_ptr>) {
            using NextValue = CallbackResult<F, T>;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<NextValue, E>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<NextValue, E>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    auto next = std::move(result).transform(std::move(f));

                    if (!next) {
                        promise->reject(std::move(next).error());
                        return;
                    }

                    if constexpr (std::is_void_v<NextValue>)
                        promise->resolve();
                    else
                        promise->resolve(*std::move(next));
                }
            );

            return std::move(future);
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

            return std::move(future).fail([=, f = std::forward<F2>(f2)](E &&error) mutable -> Future<NextValue, E> {
                if (*fallthrough)
                    return reject(std::move(error));

                return Future<NextValue, E>::rejected(std::move(error)).fail(std::move(f));
            });
        }

        template<AsyncCallback<E> F>
        auto fail(F &&f) && {
            static_assert(std::is_same_v<typename CallbackResult<F, E>::value_type, T>);
            using NextError = CallbackResult<F, E>::error_type;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<T, NextError>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<T, NextError>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    if (!result) {
                        std::invoke(std::move(f), std::move(result).error()).then(
                            [=]<typename... Args>(Args &&... args) {
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
                }
            );

            return std::move(future);
        }

        template<Callback<E> F>
            requires (!AsyncCallback<F, E> && !FallibleCallback<F, E> && !FailingCallback<F, E>)
        auto fail(F &&f) && requires std::same_as<E, std::exception_ptr> {
            static_assert(std::is_same_v<CallbackResult<F, E>, T>);

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<T>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<T>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    try {
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
                    }
                    catch (const std::exception &) {
                        promise->reject(std::current_exception());
                    }
                }
            );

            return std::move(future);
        }

        template<typename F>
            requires (
                meta::FunctionTraits<F>::Arity == 1 &&
                std::derived_from<
                    std::remove_cvref_t<typename meta::FunctionTraits<F>::template Argument<0>::Type>,
                    std::exception
                >
            )
        auto fail(F &&f) && requires std::same_as<E, std::exception_ptr> {
            using Exception = meta::FunctionTraits<F>::template Argument<0>::Type;
            static_assert(std::is_lvalue_reference_v<Exception> && std::is_const_v<std::remove_reference_t<Exception>>);

            return std::move(*this).fail([f = std::forward<F>(f)](const std::exception_ptr &ptr) mutable {
                try {
                    std::rethrow_exception(ptr);
                }
                catch (const std::remove_cvref_t<Exception> &e) {
                    return std::invoke(std::move(f), e);
                }
            });
        }

        template<FallibleCallback<E> F>
        auto fail(F &&f) && requires (!std::same_as<E, std::exception_ptr>) {
            static_assert(std::is_same_v<typename CallbackResult<F, E>::value_type, T>);
            using NextError = CallbackResult<F, E>::error_type;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<T, NextError>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<T, NextError>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    auto next = std::move(result).or_else(std::move(f));

                    if (!next) {
                        promise->reject(std::move(next).error());
                        return;
                    }

                    if constexpr (std::is_void_v<T>)
                        promise->resolve();
                    else
                        promise->resolve(*std::move(next));
                }
            );

            return std::move(future);
        }

        template<FailingCallback<E> F>
        auto fail(F &&f) && requires (!std::same_as<E, std::exception_ptr>) {
            using NextError = std::remove_cvref_t<decltype(std::declval<CallbackResult<F, E>>().error())>;

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<T, NextError>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<T, NextError>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
                    if (!result) {
                        promise->reject(std::invoke(std::move(f), std::move(result).error()).error());
                        return;
                    }

                    if constexpr (std::is_void_v<T>)
                        promise->resolve();
                    else
                        promise->resolve(*std::move(result));
                }
            );

            return std::move(future);
        }

        template<Callback<E> F>
            requires (!AsyncCallback<F, E> && !FallibleCallback<F, E> && !FailingCallback<F, E>)
        auto fail(F &&f) && requires (!std::same_as<E, std::exception_ptr>) {
            static_assert(std::is_same_v<CallbackResult<F, E>, T>);

            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<T, E>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<T, E>>(std::move(promise)), f = std::forward<F>(f)
                ](std::expected<T, E> &&result) mutable {
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
                }
            );

            return std::move(future);
        }

        auto finally(std::function<void()> f) && {
            assert(this->mCore);
            assert(!this->mCore->callback);
            assert(this->mCore->state != State::OnlyCallback);
            assert(this->mCore->state != State::Done);

            auto [promise, future] = makeContract<T, E>(this->mCore->executor);

            setCallback(
                [
                    promise = std::make_shared<Promise<T, E>>(std::move(promise)), f = std::move(f)
                ](std::expected<T, E> &&result) mutable {
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
                }
            );

            return std::move(future);
        }
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
    auto allSettled(Future<Ts, Es>... futures) {
        return allSettled(std::index_sequence_for<Ts...>{}, std::move(futures)...);
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
