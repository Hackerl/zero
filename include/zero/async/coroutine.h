#ifndef ZERO_COROUTINE_H
#define ZERO_COROUTINE_H

#include "promise.h"
#include <optional>
#include <coroutine>
#include <system_error>
#include <source_location>

#define CO_CANCELLED() (co_await zero::async::coroutine::CheckPoint{})

namespace zero::async::coroutine {
    template<typename T, typename E>
    struct Awaitable {
        [[nodiscard]] bool await_ready() const {
            return promise.status() != promise::PENDING;
        }

        void await_suspend(const std::coroutine_handle<> handle) {
            if constexpr (std::is_void_v<T>) {
                promise.then(
                    [=] {
                        handle.resume();
                    },
                    [=](const E &) {
                        handle.resume();
                    }
                );
            }
            else {
                promise.then(
                    [=](const T &) {
                        handle.resume();
                    },
                    [=](const E &) {
                        handle.resume();
                    }
                );
            }
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        tl::expected<T, E> &await_resume() {
            return promise.result();
        }

        template<typename = void>
            requires std::is_same_v<E, std::exception_ptr>
        std::add_lvalue_reference_t<T> await_resume() {
            auto &result = promise.result();

            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                return *result;
            else
                return;
        }

        promise::Promise<T, E> promise;
    };

    template<typename T, typename E>
    struct NoExceptAwaitable {
        [[nodiscard]] bool await_ready() const {
            return promise.status() != promise::PENDING;
        }

        void await_suspend(const std::coroutine_handle<> handle) {
            if constexpr (std::is_void_v<T>) {
                promise.then(
                    [=] {
                        handle.resume();
                    },
                    [=](const E &) {
                        handle.resume();
                    }
                );
            }
            else {
                promise.then(
                    [=](const T &) {
                        handle.resume();
                    },
                    [=](const E &) {
                        handle.resume();
                    }
                );
            }
        }

        tl::expected<T, E> &await_resume() {
            return promise.result();
        }

        promise::Promise<T, E> promise;
    };

    template<typename T, typename E>
    class Promise;

    struct Frame {
        std::shared_ptr<Frame> next;
        std::optional<std::source_location> location;
        std::function<tl::expected<void, std::error_code>()> cancel;
        bool cancelled{};
    };

    template<typename T, typename E>
    struct Cancellable {
        promise::Promise<T, E> promise;
        std::function<tl::expected<void, std::error_code>()> cancel;
    };

    template<typename T, typename E>
    Cancellable(promise::Promise<T, E>, std::function<void()>) -> Cancellable<T, E>;

    struct CheckPoint {
    };

    template<typename T, typename E = std::exception_ptr>
    class Task {
    public:
        using value_type = T;
        using error_type = E;
        using promise_type = Promise<T, E>;

        explicit Task(promise_type promise) : mPromise(std::move(promise)) {
        }

        tl::expected<void, std::error_code> cancel() {
            auto frame = mPromise.mFrame;
            frame->cancelled = true;

            while (frame->next) {
                frame = frame->next;
                frame->cancelled = true;
            }

            if (!frame->cancel)
                return tl::unexpected(make_error_code(std::errc::operation_not_supported));

            const auto result = std::exchange(frame->cancel, nullptr)();

            if (!result)
                return tl::unexpected(result.error());

            return {};
        }

        [[nodiscard]] std::vector<std::source_location> traceback() const {
            std::vector<std::source_location> callstack;

            for (auto frame = std::as_const(mPromise.mFrame); frame; frame = frame->next) {
                if (!frame->location)
                    break;

                callstack.push_back(*frame->location);
            }

            return callstack;
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (!std::is_same_v<E, std::exception_ptr> && std::is_void_v<T>)
        Task<typename R::value_type, E> andThen(F f) && {
            auto result = std::move(co_await *this);

            if constexpr (detail::is_specialization<R, Task>) {
                if (!result)
                    co_return tl::unexpected(std::move(result.error()));

                co_return std::move(co_await std::invoke(std::move(f)));
            }
            else {
                co_return std::move(result).and_then(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (!std::is_same_v<E, std::exception_ptr> && std::is_void_v<T>)
        Task<typename R::value_type, E> andThen(F f) & {
            auto result = co_await *this;

            if constexpr (detail::is_specialization<R, Task>) {
                if (!result)
                    co_return tl::unexpected(std::move(result.error()));

                co_return std::move(co_await std::invoke(std::move(f)));
            }
            else {
                co_return std::move(result).and_then(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, T>>
            requires (!std::is_same_v<E, std::exception_ptr> && !std::is_void_v<T>)
        Task<typename R::value_type, E> andThen(F f) && {
            auto result = std::move(co_await *this);

            if constexpr (detail::is_specialization<R, Task>) {
                if (!result)
                    co_return tl::unexpected(std::move(result.error()));

                co_return std::move(co_await std::invoke(std::move(f), std::move(*result)));
            }
            else {
                co_return std::move(result).and_then(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, T>>
            requires (!std::is_same_v<E, std::exception_ptr> && !std::is_void_v<T>)
        Task<typename R::value_type, E> andThen(F f) & {
            auto result = co_await *this;

            if constexpr (detail::is_specialization<R, Task>) {
                if (!result)
                    co_return tl::unexpected(std::move(result.error()));

                co_return std::move(co_await std::invoke(std::move(f), std::move(*result)));
            }
            else {
                co_return std::move(result).and_then(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                std::is_void_v<T> &&
                !detail::is_specialization<R, Task>
            )
        Task<R, E> transform(F f) && {
            auto result = std::move(co_await *this);
            co_return std::move(result).transform(std::move(f));
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                std::is_void_v<T> &&
                !detail::is_specialization<R, Task>
            )
        Task<R, E> transform(F f) & {
            auto result = co_await *this;
            co_return std::move(result).transform(std::move(f));
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                std::is_void_v<T> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<typename R::value_type, E> transform(F f) && {
            auto result = std::move(co_await *this);

            if (!result)
                co_return tl::unexpected(std::move(result.error()));

            co_return std::move(co_await std::invoke(std::move(f)));
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                std::is_void_v<T> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<typename R::value_type, E> transform(F f) & {
            auto result = co_await *this;

            if (!result)
                co_return tl::unexpected(std::move(result.error()));

            co_return std::move(co_await std::invoke(std::move(f)));
        }

        template<typename F>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                !std::is_void_v<T> &&
                !detail::is_specialization<std::invoke_result_t<F, T>, Task>
            )
        Task<std::invoke_result_t<F, T>, E> transform(F f) && {
            auto result = std::move(co_await *this);
            co_return std::move(result).transform(std::move(f));
        }

        template<typename F>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                !std::is_void_v<T> &&
                !detail::is_specialization<std::invoke_result_t<F, T>, Task>
            )
        Task<std::invoke_result_t<F, T>, E> transform(F f) & {
            auto result = co_await *this;
            co_return std::move(result).transform(std::move(f));
        }

        template<typename F, typename R = std::invoke_result_t<F, T>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                !std::is_void_v<T> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<typename R::value_type, E> transform(F f) && {
            auto result = std::move(co_await *this);

            if (!result)
                co_return tl::unexpected(std::move(result.error()));

            if constexpr (std::is_void_v<typename R::value_type>) {
                co_await std::invoke(std::move(f), std::move(*result));
                co_return tl::expected<typename R::value_type, E>{};
            }
            else {
                co_return std::move(co_await std::invoke(std::move(f), std::move(*result)));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, T>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                !std::is_void_v<T> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<typename R::value_type, E> transform(F f) & {
            auto result = co_await *this;

            if (!result)
                co_return tl::unexpected(std::move(result.error()));

            if constexpr (std::is_void_v<typename R::value_type>) {
                co_await std::invoke(std::move(f), std::move(*result));
                co_return tl::expected<typename R::value_type, E>{};
            }
            else {
                co_return std::move(co_await std::invoke(std::move(f), std::move(*result)));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (!std::is_same_v<E, std::exception_ptr>)
        Task<T, typename R::error_type> orElse(F f) && {
            auto result = std::move(co_await *this);

            if constexpr (detail::is_specialization<R, Task>) {
                if (result)
                    co_return std::move(*result);

                co_return std::move(co_await std::invoke(std::move(f), std::move(result.error())));
            }
            else {
                co_return std::move(result).or_else(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (!std::is_same_v<E, std::exception_ptr>)
        Task<T, typename R::error_type> orElse(F f) & {
            auto result = co_await *this;

            if constexpr (detail::is_specialization<R, Task>) {
                if (result)
                    co_return std::move(*result);

                co_return std::move(co_await std::invoke(std::move(f), std::move(result.error())));
            }
            else {
                co_return std::move(result).or_else(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (!std::is_same_v<E, std::exception_ptr> && !detail::is_specialization<R, Task>)
        Task<T, R> transformError(F f) && {
            auto result = std::move(co_await *this);
            co_return std::move(result).transform_error(std::move(f));
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (!std::is_same_v<E, std::exception_ptr> && !detail::is_specialization<R, Task>)
        Task<T, R> transformError(F f) & {
            auto result = co_await *this;
            co_return std::move(result).transform_error(std::move(f));
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<T, typename R::value_type> transformError(F f) && {
            auto result = std::move(co_await *this);

            if (result)
                co_return std::move(*result);

            co_return tl::unexpected(std::move(co_await std::invoke(std::move(f), std::move(result.error()))));
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<T, typename R::value_type> transformError(F f) & {
            auto result = co_await *this;

            if (result)
                co_return std::move(*result);

            co_return tl::unexpected(std::move(co_await std::invoke(std::move(f), std::move(result.error()))));
        }

        promise::Promise<T, E> &promise() {
            return mPromise;
        }

        [[nodiscard]] const promise::Promise<T, E> &promise() const {
            return mPromise;
        }

        [[nodiscard]] bool done() const {
            return mPromise.status() != promise::PENDING;
        }

        [[nodiscard]] const tl::expected<T, E> &result() const {
            return mPromise.result();
        }

    private:
        promise_type mPromise;

        template<typename, typename>
        friend class Promise;
    };

    template<typename T, typename E>
    class Promise : public promise::Promise<T, E> {
    public:
        Promise() : mFrame(std::make_shared<Frame>()) {
        }

        std::suspend_never initial_suspend() {
            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() {
            if constexpr (std::is_same_v<E, std::exception_ptr>)
                promise::Promise<T, E>::reject(std::current_exception());
            else
                std::rethrow_exception(std::current_exception());
        }

        Task<T, E> get_return_object() {
            return Task<T, E>{*this};
        }

        [[nodiscard]] Awaitable<bool, std::exception_ptr> await_transform(CheckPoint) const {
            if (!mFrame->cancelled)
                return {promise::resolve<bool, std::exception_ptr>(false)};

            return {promise::resolve<bool, std::exception_ptr>(true)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            Cancellable<Result, Error> &&cancellable,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = std::move(cancellable.cancel);

            return {std::move(cancellable.promise)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            const Cancellable<Result, Error> &cancellable,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = cancellable.cancel;

            return {cancellable.promise};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            promise::Promise<Result, Error> &&promise,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {std::move(promise)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            const promise::Promise<Result, Error> &promise,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {promise};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
            Task<Result, Error> &&task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mPromise.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {std::move(task.mPromise)};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
            const Task<Result, Error> &task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mPromise.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.mPromise};
        }

        template<typename U>
            requires (
                !std::is_void_v<T> &&
                !detail::is_specialization<std::decay_t<U>, tl::expected> &&
                !detail::is_specialization<std::decay_t<U>, tl::unexpected>
            )
        void return_value(U &&result) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            promise::Promise<T, E>::resolve(std::forward<U>(result));
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(const tl::expected<T, E> &result) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            if (!result) {
                promise::Promise<T, E>::reject(result.error());
                return;
            }

            if constexpr (std::is_void_v<T>)
                promise::Promise<T, E>::resolve();
            else
                promise::Promise<T, E>::resolve(result.value());
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(tl::expected<T, E> &&result) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            if (!result) {
                promise::Promise<T, E>::reject(std::move(result.error()));
                return;
            }

            if constexpr (std::is_void_v<T>)
                promise::Promise<T, E>::resolve();
            else
                promise::Promise<T, E>::resolve(std::move(result.value()));
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(const tl::unexpected<E> &reason) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            promise::Promise<T, E>::reject(reason.value());
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(tl::unexpected<E> &&reason) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            promise::Promise<T, E>::reject(std::move(reason.value()));
        }

    private:
        std::shared_ptr<Frame> mFrame;

        template<typename, typename>
        friend class Task;

        template<typename, typename>
        friend class Promise;
    };

    template<>
    class Promise<void, std::exception_ptr> : public promise::Promise<void, std::exception_ptr> {
    public:
        Promise() : mFrame(std::make_shared<Frame>()) {
        }

        std::suspend_never initial_suspend() {
            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() {
            promise::Promise<void, std::exception_ptr>::reject(std::current_exception());
        }

        [[nodiscard]] Awaitable<bool, std::exception_ptr> await_transform(CheckPoint) const {
            if (!mFrame->cancelled)
                return {promise::resolve<bool, std::exception_ptr>(false)};

            return {promise::resolve<bool, std::exception_ptr>(true)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            Cancellable<Result, Error> &&cancellable,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = std::move(cancellable.cancel);

            return {std::move(cancellable.promise)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            const Cancellable<Result, Error> &cancellable,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = cancellable.cancel;

            return {cancellable.promise};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            promise::Promise<Result, Error> &&promise,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {std::move(promise)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error> await_transform(
            const promise::Promise<Result, Error> &promise,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {promise};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
            Task<Result, Error> &&task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mPromise.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {std::move(task.mPromise)};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
            const Task<Result, Error> &task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mPromise.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.mPromise};
        }

        [[nodiscard]] Task<void> get_return_object() const {
            return Task{*this};
        }

        void return_void() {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            promise::Promise<void, std::exception_ptr>::resolve();
        }

    private:
        std::shared_ptr<Frame> mFrame;

        template<typename, typename>
        friend class Task;

        template<typename, typename>
        friend class Promise;
    };

    template<typename... Ts, typename E, typename T = promise::promises_result_t<Ts...>>
    Task<T, E> all(Task<Ts, E>... tasks) {
        auto result = std::move(co_await Cancellable{
            promise::all(tasks.promise()...),
            [=]() mutable {
                std::optional<tl::expected<void, std::error_code>> res;

                ([&] {
                    if (res && *res)
                        return;

                    if (tasks.done())
                        return;

                    res = tasks.cancel();
                }(), ...);

                return *res;
            }
        });

        if (!result) {
            ([&] {
                if (tasks.done())
                    return;

                tasks.cancel();
            }(), ...);
        }

        if constexpr (std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                co_return std::move(*result);
        }
        else {
            co_return std::move(result);
        }
    }

    template<typename... Ts, typename... E>
    Task<std::tuple<tl::expected<Ts, E>...>> allSettled(Task<Ts, E>... tasks) {
        co_return *std::move(co_await Cancellable{
            promise::allSettled(tasks.promise()...),
            [=]() mutable -> tl::expected<void, std::error_code> {
                std::error_code ec;

                ([&] {
                    if (tasks.done())
                        return;

                    if (const auto result = tasks.cancel(); !result)
                        ec = result.error();
                }(), ...);

                if (ec)
                    return tl::unexpected(ec);

                return {};
            }
        });
    }

    template<typename... Ts,
             typename E,
             typename F = detail::first_element_t<Ts...>,
             typename T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>>
    Task<T, std::list<E>> any(Task<Ts, E>... tasks) {
        auto result = std::move(co_await Cancellable{
            promise::any(tasks.promise()...),
            [=]() mutable -> tl::expected<void, std::error_code> {
                std::error_code ec;

                ([&] {
                    if (tasks.done())
                        return;

                    if (const auto res = tasks.cancel(); !res)
                        ec = res.error();
                }(), ...);

                if (ec)
                    return tl::unexpected(ec);

                return {};
            }
        });

        if (result)
            ([&] {
                if (tasks.done())
                    return;

                tasks.cancel();
            }(), ...);

        co_return std::move(result);
    }

    template<typename... Ts,
             typename E,
             typename F = detail::first_element_t<Ts...>,
             typename T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>>
    Task<T, E> race(Task<Ts, E>... tasks) {
        auto result = std::move(co_await Cancellable{
            promise::race(tasks.promise()...),
            [=]() mutable {
                std::optional<tl::expected<void, std::error_code>> res;

                ([&] {
                    if (res && *res)
                        return;

                    if (tasks.done())
                        return;

                    res = tasks.cancel();
                }(), ...);

                return *res;
            }
        });

        ([&] {
            if (tasks.done())
                return;

            tasks.cancel();
        }(), ...);

        if constexpr (std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                co_return std::move(*result);
        }
        else {
            co_return std::move(result);
        }
    }

    template<typename T, typename E>
    Task<T, E> from(promise::Promise<T, E> promise) {
        auto result = std::move(co_await std::move(promise));

        if constexpr (std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                co_return std::move(*result);
        }
        else {
            co_return std::move(result);
        }
    }

    template<typename T, typename E>
    Task<T, E> from(Cancellable<T, E> cancellable) {
        auto result = std::move(co_await std::move(cancellable));

        if constexpr (std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                co_return std::move(*result);
        }
        else {
            co_return std::move(result);
        }
    }
}

#endif //ZERO_COROUTINE_H
