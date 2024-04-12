#ifndef ZERO_COROUTINE_H
#define ZERO_COROUTINE_H

#include "promise.h"
#include <optional>
#include <coroutine>
#include <system_error>
#include <source_location>

namespace zero::async::coroutine {
    template<typename T, typename E>
    struct Awaitable {
        [[nodiscard]] bool await_ready() {
            if (!future.isReady())
                return false;

            result.emplace(std::move(future).result());
            return true;
        }

        void await_suspend(const std::coroutine_handle<> handle) {
            future.setCallback([=, this](tl::expected<T, E> res) {
                if (!res) {
                    result.emplace(tl::unexpected(std::move(res).error()));
                    handle.resume();
                    return;
                }

                if constexpr (std::is_void_v<T>)
                    result.emplace();
                else
                    result.emplace(*std::move(res));

                handle.resume();
            });
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        tl::expected<T, E> await_resume() {
            return std::move(*result);
        }

        template<typename = void>
            requires std::is_same_v<E, std::exception_ptr>
        T await_resume() {
            if (!result->has_value())
                std::rethrow_exception(result->error());

            if constexpr (!std::is_void_v<T>)
                return *std::move(*result);
            else
                return;
        }

        promise::Future<T, E> future;
        std::optional<tl::expected<T, E>> result;
    };

    template<typename T, typename E>
    struct NoExceptAwaitable {
        [[nodiscard]] bool await_ready() {
            if (!future.isReady())
                return false;

            result.emplace(std::move(future).result());
            return true;
        }

        void await_suspend(const std::coroutine_handle<> handle) {
            future.setCallback([=, this](tl::expected<T, E> res) {
                if (!res) {
                    result.emplace(tl::unexpected(std::move(res).error()));
                    handle.resume();
                    return;
                }

                if constexpr (std::is_void_v<T>)
                    result.emplace();
                else
                    result.emplace(*std::move(res));

                handle.resume();
            });
        }

        tl::expected<T, E> await_resume() {
            return std::move(*result);
        }

        promise::Future<T, E> future;
        std::optional<tl::expected<T, E>> result;
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
        promise::Future<T, E> future;
        std::function<tl::expected<void, std::error_code>()> cancel;
    };

    template<typename T, typename E>
    Cancellable(promise::Future<T, E>, std::function<void()>) -> Cancellable<T, E>;

    struct Cancelled {
    };

    inline constexpr Cancelled cancelled;

    template<typename T, typename E = std::exception_ptr>
    class Task {
    public:
        using value_type = T;
        using error_type = E;
        using promise_type = Promise<T, E>;

        Task(std::shared_ptr<Frame> frame, std::shared_ptr<promise::Promise<T, E>> promise)
            : mFrame(std::move(frame)), mPromise(std::move(promise)) {
        }

        Task(Task &&rhs) noexcept : mFrame(std::move(rhs.mFrame)), mPromise(std::move(rhs.mPromise)) {
        }

        Task &operator=(Task &&rhs) noexcept {
            mFrame = std::move(rhs.mFrame);
            mPromise = std::move(rhs.mPromise);
            return *this;
        }

        // ReSharper disable once CppMemberFunctionMayBeConst
        tl::expected<void, std::error_code> cancel() {
            auto frame = mFrame;
            frame->cancelled = true;

            while (frame->next) {
                frame = frame->next;
                frame->cancelled = true;
            }

            if (!frame->cancel)
                return tl::unexpected(make_error_code(std::errc::operation_not_supported));

            if (const auto result = std::exchange(frame->cancel, nullptr)(); !result)
                return tl::unexpected(result.error());

            return {};
        }

        [[nodiscard]] std::vector<std::source_location> traceback() const {
            std::vector<std::source_location> callstack;

            for (auto frame = mFrame; frame; frame = frame->next) {
                if (!frame->location)
                    break;

                callstack.push_back(*frame->location);
            }

            return callstack;
        }

        template<typename F, typename R = std::invoke_result_t<F>>
            requires (!std::is_same_v<E, std::exception_ptr> && std::is_void_v<T>)
        Task<typename R::value_type, E> andThen(F f) && {
            auto result = co_await std::move(*this);

            if constexpr (detail::is_specialization<R, Task>) {
                if (!result)
                    co_return tl::unexpected(std::move(result).error());

                co_return co_await std::invoke(std::move(f));
            }
            else {
                co_return std::move(result).and_then(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, T>>
            requires (!std::is_same_v<E, std::exception_ptr> && !std::is_void_v<T>)
        Task<typename R::value_type, E> andThen(F f) && {
            auto result = co_await std::move(*this);

            if constexpr (detail::is_specialization<R, Task>) {
                if (!result)
                    co_return tl::unexpected(std::move(result).error());

                co_return co_await std::invoke(std::move(f), *std::move(result));
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
            auto result = co_await std::move(*this);
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
            auto result = co_await std::move(*this);

            if (!result)
                co_return tl::unexpected(std::move(result).error());

            co_return co_await std::invoke(std::move(f));
        }

        template<typename F>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                !std::is_void_v<T> &&
                !detail::is_specialization<std::invoke_result_t<F, T>, Task>
            )
        Task<std::invoke_result_t<F, T>, E> transform(F f) && {
            auto result = co_await std::move(*this);
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
            auto result = co_await std::move(*this);

            if (!result)
                co_return tl::unexpected(std::move(result).error());

            if constexpr (std::is_void_v<typename R::value_type>) {
                co_await std::invoke(std::move(f), *std::move(result));
                co_return tl::expected<typename R::value_type, E>{};
            }
            else {
                co_return co_await std::invoke(std::move(f), *std::move(result));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (!std::is_same_v<E, std::exception_ptr>)
        Task<T, typename R::error_type> orElse(F f) && {
            auto result = co_await std::move(*this);

            if constexpr (detail::is_specialization<R, Task>) {
                if (result)
                    co_return *std::move(result);

                co_return co_await std::invoke(std::move(f), std::move(result).error());
            }
            else {
                co_return std::move(result).or_else(std::move(f));
            }
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (!std::is_same_v<E, std::exception_ptr> && !detail::is_specialization<R, Task>)
        Task<T, R> transformError(F f) && {
            auto result = co_await std::move(*this);
            co_return std::move(result).transform_error(std::move(f));
        }

        template<typename F, typename R = std::invoke_result_t<F, E>>
            requires (
                !std::is_same_v<E, std::exception_ptr> &&
                detail::is_specialization<R, Task> &&
                std::is_same_v<typename R::error_type, std::exception_ptr>
            )
        Task<T, typename R::value_type> transformError(F f) && {
            auto result = co_await std::move(*this);

            if (result)
                co_return *std::move(result);

            co_return tl::unexpected(co_await std::invoke(std::move(f), std::move(result).error()));
        }

        [[nodiscard]] bool done() const {
            return mPromise->isFulfilled();
        }

        [[nodiscard]] bool cancelled() const {
            return mFrame->cancelled;
        }

        promise::Future<T, E> future() {
            return mPromise->getFuture();
        }

    private:
        std::shared_ptr<Frame> mFrame;
        std::shared_ptr<promise::Promise<T, E>> mPromise;

        template<typename, typename>
        friend class Promise;
    };

    template<typename T, typename E>
    class Promise {
    public:
        Promise() : mFrame(std::make_shared<Frame>()), mPromise(std::make_shared<promise::Promise<T, E>>()) {
        }

        std::suspend_never initial_suspend() {
            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() {
            if constexpr (std::is_same_v<E, std::exception_ptr>)
                mPromise->reject(std::current_exception());
            else
                std::rethrow_exception(std::current_exception());
        }

        Task<T, E> get_return_object() {
            return {mFrame, mPromise};
        }

        [[nodiscard]] Awaitable<bool, std::exception_ptr> await_transform(Cancelled) const {
            if (!mFrame->cancelled)
                return {promise::resolve<bool, std::exception_ptr>(false)};

            return {promise::resolve<bool, std::exception_ptr>(true)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error>
        await_transform(
            Cancellable<Result, Error> cancellable,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = std::move(cancellable.cancel);

            return {std::move(cancellable.future)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error>
        await_transform(
            promise::Future<Result, Error> future,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {std::move(future)};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error>
        await_transform(
            Task<Result, Error> &&task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.future()};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error>
        await_transform(
            Task<Result, Error> &task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.future()};
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

            mPromise->resolve(std::forward<U>(result));
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(const tl::expected<T, E> &result) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            if (!result) {
                mPromise->reject(result.error());
                return;
            }

            if constexpr (std::is_void_v<T>)
                mPromise->resolve();
            else
                mPromise->resolve(result.value());
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(tl::expected<T, E> &&result) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            if (!result) {
                mPromise->reject(std::move(result).error());
                return;
            }

            if constexpr (std::is_void_v<T>)
                mPromise->resolve();
            else
                mPromise->resolve(*std::move(result));
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(const tl::unexpected<E> &reason) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            mPromise->reject(reason.value());
        }

        template<typename = void>
            requires (!std::is_same_v<E, std::exception_ptr>)
        void return_value(tl::unexpected<E> &&reason) {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            mPromise->reject(std::move(reason.value()));
        }

    private:
        std::shared_ptr<Frame> mFrame;
        std::shared_ptr<promise::Promise<T, E>> mPromise;
    };

    template<>
    class Promise<void, std::exception_ptr> {
    public:
        Promise(): mFrame(std::make_shared<Frame>()),
                   mPromise(std::make_shared<promise::Promise<void, std::exception_ptr>>()) {
        }

        std::suspend_never initial_suspend() {
            return {};
        }

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() {
            mPromise->reject(std::current_exception());
        }

        [[nodiscard]] Task<void> get_return_object() {
            return {mFrame, mPromise};
        }

        [[nodiscard]] Awaitable<bool, std::exception_ptr> await_transform(Cancelled) const {
            if (!mFrame->cancelled)
                return {promise::resolve<bool, std::exception_ptr>(false)};

            return {promise::resolve<bool, std::exception_ptr>(true)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error>
        await_transform(
            Cancellable<Result, Error> cancellable,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = std::move(cancellable.cancel);

            return {std::move(cancellable.future)};
        }

        template<typename Result, typename Error>
        NoExceptAwaitable<Result, Error>
        await_transform(
            promise::Future<Result, Error> future,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {std::move(future)};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error>
        await_transform(
            Task<Result, Error> &&task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.future()};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error>
        await_transform(
            Task<Result, Error> &task,
            const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.future()};
        }

        void return_void() {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            mPromise->resolve();
        }

    private:
        std::shared_ptr<Frame> mFrame;
        std::shared_ptr<promise::Promise<void, std::exception_ptr>> mPromise;
    };

    template<typename... Ts, typename E>
    auto all(Task<Ts, E>... tasks) {
        using T = promise::futures_result_t<Ts...>;

        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<T, E> {
            auto result = co_await Cancellable{
                promise::all(taskPtrs->future()...),
                [=] {
                    std::optional<tl::expected<void, std::error_code>> res;

                    ([&] {
                        if (res && *res)
                            return;

                        if (taskPtrs->done())
                            return;

                        res = taskPtrs->cancel();
                    }(), ...);

                    return *res;
                }
            };

            if (!result) {
                ([&] {
                    if (taskPtrs->done())
                        return;

                    taskPtrs->cancel();
                }(), ...);
            }

            if constexpr (std::is_same_v<E, std::exception_ptr>) {
                if (!result)
                    std::rethrow_exception(result.error());

                if constexpr (!std::is_void_v<T>)
                    co_return *std::move(result);
            }
            else {
                co_return std::move(result);
            }
        }(std::make_shared<Task<Ts, E>>(std::move(tasks))...);
    }

    template<typename... Ts, typename... E>
    auto allSettled(Task<Ts, E>... tasks) {
        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<std::tuple<tl::expected<Ts, E>...>> {
            co_return *co_await Cancellable{
                promise::allSettled(taskPtrs->future()...),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    ([&] {
                        if (taskPtrs->done())
                            return;

                        if (const auto result = taskPtrs->cancel(); !result)
                            ec = result.error();
                    }(), ...);

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };
        }(std::make_shared<Task<Ts, E>>(std::move(tasks))...);
    }

    template<typename... Ts, typename E>
    auto any(Task<Ts, E>... tasks) {
        using F = detail::first_element_t<Ts...>;
        using T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>;

        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<T, std::array<E, sizeof...(Ts)>> {
            auto result = co_await Cancellable{
                promise::any(taskPtrs->future()...),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    ([&] {
                        if (taskPtrs->done())
                            return;

                        if (const auto res = taskPtrs->cancel(); !res)
                            ec = res.error();
                    }(), ...);

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            if (result)
                ([&] {
                    if (taskPtrs->done())
                        return;

                    taskPtrs->cancel();
                }(), ...);

            co_return std::move(result);
        }(std::make_shared<Task<Ts, E>>(std::move(tasks))...);
    }

    template<typename... Ts, typename E>
    auto race(Task<Ts, E>... tasks) {
        using F = detail::first_element_t<Ts...>;
        using T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>;

        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<T, E> {
            auto result = co_await Cancellable{
                promise::race(taskPtrs->future()...),
                [=] {
                    std::optional<tl::expected<void, std::error_code>> res;

                    ([&] {
                        if (res && *res)
                            return;

                        if (taskPtrs->done())
                            return;

                        res = taskPtrs->cancel();
                    }(), ...);

                    return *res;
                }
            };

            ([&] {
                if (taskPtrs->done())
                    return;

                taskPtrs->cancel();
            }(), ...);

            if constexpr (std::is_same_v<E, std::exception_ptr>) {
                if (!result)
                    std::rethrow_exception(result.error());

                if constexpr (!std::is_void_v<T>)
                    co_return *std::move(result);
            }
            else {
                co_return std::move(result);
            }
        }(std::make_shared<Task<Ts, E>>(std::move(tasks))...);
    }

    template<typename T, typename E>
    Task<T, E> from(promise::Future<T, E> future) {
        auto result = co_await std::move(future);

        if constexpr (std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                co_return *std::move(result);
        }
        else {
            co_return std::move(result);
        }
    }

    template<typename T, typename E>
    Task<T, E> from(Cancellable<T, E> cancellable) {
        auto result = co_await std::move(cancellable);

        if constexpr (std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            if constexpr (!std::is_void_v<T>)
                co_return *std::move(result);
        }
        else {
            co_return std::move(result);
        }
    }
}

#endif //ZERO_COROUTINE_H
