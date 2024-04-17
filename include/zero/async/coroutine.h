#ifndef ZERO_COROUTINE_H
#define ZERO_COROUTINE_H

#include "promise.h"
#include <list>
#include <optional>
#include <algorithm>
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
            return {promise::resolve<bool, std::exception_ptr>(mFrame->cancelled)};
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

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization<std::iter_value_t<I>, Task>
    auto all(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        std::list<std::shared_ptr<Task<T, E>>> ptrs;
        std::transform(
            std::move(first),
            std::move(last),
            std::back_inserter(ptrs),
            [](Task<T, E> &&task) {
                return std::make_shared<Task<T, E>>(std::move(task));
            }
        );

        return [](auto taskPtrs) -> Task<std::conditional_t<std::is_void_v<T>, void, std::vector<T>>, E> {
            std::list<promise::Future<void>> futures;

            auto result = co_await Cancellable{
                promise::all(
                    taskPtrs | std::views::transform([](auto &taskPtr) {
                        return taskPtr->future();
                    }),
                    std::back_inserter(futures)
                ),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    for (const auto &taskPtr: taskPtrs) {
                        if (taskPtr->done() || taskPtr->cancelled())
                            continue;

                        if (const auto res = taskPtr->cancel(); !res)
                            ec = res.error();
                    }

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            if (!result) {
                for (const auto &taskPtr: taskPtrs) {
                    if (taskPtr->done() || taskPtr->cancelled())
                        continue;

                    taskPtr->cancel();
                }
            }

            co_await allSettled(std::move(futures));

            if constexpr (std::is_same_v<E, std::exception_ptr>) {
                if (!result)
                    std::rethrow_exception(result.error());

                if constexpr (!std::is_void_v<T>)
                    co_return *std::move(result);
            }
            else {
                co_return std::move(result);
            }
        }(std::move(ptrs));
    }

    template<std::ranges::range R>
        requires detail::is_specialization<std::ranges::range_value_t<R>, Task>
    auto all(R tasks) {
        return all(std::make_move_iterator(tasks.begin()), std::make_move_iterator(tasks.end()));
    }

    template<typename... Ts, typename E>
    auto all(Task<Ts, E>... tasks) {
        using T = promise::futures_result_t<Ts...>;

        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<T, E> {
            std::list<promise::Future<void>> futures;

            auto result = co_await Cancellable{
                promise::all(std::back_inserter(futures), taskPtrs->future()...),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    ([&] {
                        if (taskPtrs->done() || taskPtrs->cancelled())
                            return;

                        if (const auto res = taskPtrs->cancel(); !res)
                            ec = res.error();
                    }(), ...);

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            if (!result) {
                ([&] {
                    if (taskPtrs->done() || taskPtrs->cancelled())
                        return;

                    taskPtrs->cancel();
                }(), ...);
            }

            co_await allSettled(std::move(futures));

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

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization<std::iter_value_t<I>, Task>
    auto allSettled(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        std::list<std::shared_ptr<Task<T, E>>> ptrs;
        std::transform(
            std::move(first),
            std::move(last),
            std::back_inserter(ptrs),
            [](Task<T, E> &&task) {
                return std::make_shared<Task<T, E>>(std::move(task));
            }
        );

        return [](auto taskPtrs) -> Task<std::vector<tl::expected<T, E>>> {
            co_return *co_await Cancellable{
                promise::allSettled(taskPtrs | std::views::transform([](auto &taskPtr) { return taskPtr->future(); })),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    for (const auto &taskPtr: taskPtrs) {
                        if (taskPtr->done() || taskPtr->cancelled())
                            continue;

                        if (const auto result = taskPtr->cancel(); !result)
                            ec = result.error();
                    }

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };
        }(std::move(ptrs));
    }

    template<std::ranges::range R>
        requires detail::is_specialization<std::ranges::range_value_t<R>, Task>
    auto allSettled(R tasks) {
        return allSettled(std::make_move_iterator(tasks.begin()), std::make_move_iterator(tasks.end()));
    }

    template<typename... Ts, typename... Es>
    auto allSettled(Task<Ts, Es>... tasks) {
        using T = std::conditional_t<
            detail::all_same_v<Ts...>,
            std::array<tl::expected<detail::first_element_t<Ts...>, detail::first_element_t<Es...>>, sizeof...(Ts)>,
            std::tuple<tl::expected<Ts, Es>...>
        >;

        return [](std::shared_ptr<Task<Ts, Es>>... taskPtrs) -> Task<T> {
            co_return *co_await Cancellable{
                promise::allSettled(taskPtrs->future()...),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    ([&] {
                        if (taskPtrs->done() || taskPtrs->cancelled())
                            return;

                        if (const auto result = taskPtrs->cancel(); !result)
                            ec = result.error();
                    }(), ...);

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };
        }(std::make_shared<Task<Ts, Es>>(std::move(tasks))...);
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization<std::iter_value_t<I>, Task>
    auto any(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        std::list<std::shared_ptr<Task<T, E>>> ptrs;
        std::transform(
            std::move(first),
            std::move(last),
            std::back_inserter(ptrs),
            [](Task<T, E> &&task) {
                return std::make_shared<Task<T, E>>(std::move(task));
            }
        );

        return [](auto taskPtrs) -> Task<T, std::vector<E>> {
            std::list<promise::Future<void>> futures;

            auto result = co_await Cancellable{
                promise::any(
                    taskPtrs | std::views::transform([](auto &taskPtr) {
                        return taskPtr->future();
                    }),
                    std::back_inserter(futures)
                ),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    for (const auto &taskPtr: taskPtrs) {
                        if (taskPtr->done() || taskPtr->cancelled())
                            continue;

                        if (const auto res = taskPtr->cancel(); !res)
                            ec = res.error();
                    }

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            if (result) {
                for (const auto &taskPtr: taskPtrs) {
                    if (taskPtr->done() || taskPtr->cancelled())
                        continue;

                    taskPtr->cancel();
                }
            }

            co_await allSettled(std::move(futures));
            co_return std::move(result);
        }(std::move(ptrs));
    }

    template<std::ranges::range R>
        requires detail::is_specialization<std::ranges::range_value_t<R>, Task>
    auto any(R tasks) {
        return any(std::make_move_iterator(tasks.begin()), std::make_move_iterator(tasks.end()));
    }

    template<typename... Ts, typename E>
    auto any(Task<Ts, E>... tasks) {
        using T = std::conditional_t<detail::all_same_v<Ts...>, detail::first_element_t<Ts...>, std::any>;

        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<T, std::array<E, sizeof...(Ts)>> {
            std::list<promise::Future<void>> futures;

            auto result = co_await Cancellable{
                promise::any(std::back_inserter(futures), taskPtrs->future()...),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    ([&] {
                        if (taskPtrs->done() || taskPtrs->cancelled())
                            return;

                        if (const auto res = taskPtrs->cancel(); !res)
                            ec = res.error();
                    }(), ...);

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            if (result) {
                ([&] {
                    if (taskPtrs->done() || taskPtrs->cancelled())
                        return;

                    taskPtrs->cancel();
                }(), ...);
            }

            co_await allSettled(std::move(futures));
            co_return std::move(result);
        }(std::make_shared<Task<Ts, E>>(std::move(tasks))...);
    }

    template<std::input_iterator I, std::sentinel_for<I> S>
        requires detail::is_specialization<std::iter_value_t<I>, Task>
    auto race(I first, S last) {
        using T = typename std::iter_value_t<I>::value_type;
        using E = typename std::iter_value_t<I>::error_type;

        std::list<std::shared_ptr<Task<T, E>>> ptrs;
        std::transform(
            std::move(first),
            std::move(last),
            std::back_inserter(ptrs),
            [](Task<T, E> &&task) {
                return std::make_shared<Task<T, E>>(std::move(task));
            }
        );

        return [](auto taskPtrs) -> Task<T, E> {
            std::list<promise::Future<void>> futures;

            auto result = co_await Cancellable{
                promise::race(
                    taskPtrs | std::views::transform([](auto &taskPtr) {
                        return taskPtr->future();
                    }),
                    std::back_inserter(futures)
                ),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    for (const auto &taskPtr: taskPtrs) {
                        if (taskPtr->done() || taskPtr->cancelled())
                            continue;

                        if (const auto res = taskPtr->cancel(); !res)
                            ec = res.error();
                    }

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            for (const auto &taskPtr: taskPtrs) {
                if (taskPtr->done() || taskPtr->cancelled())
                    continue;

                taskPtr->cancel();
            }

            co_await allSettled(std::move(futures));

            if constexpr (std::is_same_v<E, std::exception_ptr>) {
                if (!result)
                    std::rethrow_exception(result.error());

                if constexpr (!std::is_void_v<T>)
                    co_return *std::move(result);
            }
            else {
                co_return std::move(result);
            }
        }(std::move(ptrs));
    }

    template<std::ranges::range R>
        requires detail::is_specialization<std::ranges::range_value_t<R>, Task>
    auto race(R tasks) {
        return race(std::make_move_iterator(tasks.begin()), std::make_move_iterator(tasks.end()));
    }

    template<typename... Ts, typename E>
    auto race(Task<Ts, E>... tasks) {
        using T = std::conditional_t<detail::all_same_v<Ts...>, detail::first_element_t<Ts...>, std::any>;

        return [](std::shared_ptr<Task<Ts, E>>... taskPtrs) -> Task<T, E> {
            std::list<promise::Future<void>> futures;

            auto result = co_await Cancellable{
                promise::race(std::back_inserter(futures), taskPtrs->future()...),
                [=]() -> tl::expected<void, std::error_code> {
                    std::error_code ec;

                    ([&] {
                        if (taskPtrs->done() || taskPtrs->cancelled())
                            return;

                        if (const auto res = taskPtrs->cancel(); !res)
                            ec = res.error();
                    }(), ...);

                    if (ec)
                        return tl::unexpected(ec);

                    return {};
                }
            };

            ([&] {
                if (taskPtrs->done() || taskPtrs->cancelled())
                    return;

                taskPtrs->cancel();
            }(), ...);

            co_await allSettled(std::move(futures));

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
