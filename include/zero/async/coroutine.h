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
        [[nodiscard]] bool await_ready() const {
            return promise.status() != promise::PENDING;
        }

        void await_suspend(std::coroutine_handle<> handle) {
            if constexpr (std::is_void_v<T>) {
                promise.then([=]() {
                    handle.resume();
                }, [=](const E &reason) {
                    handle.resume();
                });
            } else {
                promise.then([=](const T &result) {
                    handle.resume();
                }, [=](const E &reason) {
                    handle.resume();
                });
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
    };

    template<typename T, typename E>
    struct Cancellable {
        promise::Promise<T, E> promise;
        std::function<tl::expected<void, std::error_code>()> cancel;
    };

    template<typename T, typename E>
    Cancellable(promise::Promise<T, E>, std::function<void()>) -> Cancellable<T, E>;

    template<typename T, typename E = std::exception_ptr>
    class Task {
    public:
        using promise_type = Promise<T, E>;

    public:
        explicit Task(promise_type promise) : mPromise(std::move(promise)) {

        }

    public:
        tl::expected<void, std::error_code> cancel() {
            std::shared_ptr<Frame> frame = mPromise.mFrame;

            while (frame->next)
                frame = frame->next;

            if (!frame->cancel)
                return tl::unexpected(make_error_code(std::errc::operation_not_supported));

            auto result = std::exchange(frame->cancel, nullptr)();

            if (!result)
                return tl::unexpected(result.error());

            return {};
        }

        std::vector<std::source_location> traceback() {
            std::vector<std::source_location> callstack;

            for (auto frame = mPromise.mFrame; frame; frame = frame->next) {
                if (!frame->location)
                    break;

                callstack.push_back(*frame->location);
            }

            return callstack;
        }

    public:
        Promise<T, E> &promise() {
            return mPromise;
        }

        [[nodiscard]] const promise_type &promise() const {
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
    };

    template<typename T, typename E>
    class Promise : public promise::Promise<T, E> {
    public:
        Promise() : mFrame(std::make_shared<Frame>()) {

        }

    public:
        std::suspend_never initial_suspend() {
            return {};
        };

        std::suspend_never final_suspend() noexcept {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            return {};
        }

        void unhandled_exception() {
            if constexpr (std::is_same_v<E, std::exception_ptr>) {
                promise::Promise<T, E>::reject(std::current_exception());
            } else {
                std::rethrow_exception(std::current_exception());
            }
        }

        Task<T, E> get_return_object() {
            return Task<T, E>{*this};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
                Cancellable<Result, Error> &&cancellable,
                const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = std::move(cancellable.cancel);

            return {std::move(cancellable.promise)};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
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
                const Task<Result, Error> &task,
                const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.promise().mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.promise()};
        }

        template<typename U>
        requires (
                !std::is_void_v<T> &&
                !detail::is_specialization<std::decay_t<U>, tl::expected> &&
                !detail::is_specialization<std::decay_t<U>, tl::unexpected>
        )
        void return_value(U &&result) {
            promise::Promise<T, E>::resolve(std::forward<U>(result));
        }

        void return_value(const tl::expected<T, E> &result) {
            if (!result) {
                promise::Promise<T, E>::reject(result.error());
                return;
            }

            if constexpr (std::is_void_v<T>) {
                promise::Promise<T, E>::resolve();
            } else {
                promise::Promise<T, E>::resolve(result.value());
            }
        }

        void return_value(tl::expected<T, E> &&result) {
            if (!result) {
                promise::Promise<T, E>::reject(std::move(result.error()));
                return;
            }

            if constexpr (std::is_void_v<T>) {
                promise::Promise<T, E>::resolve();
            } else {
                promise::Promise<T, E>::resolve(std::move(result.value()));
            }
        }

        void return_value(const tl::unexpected<E> &reason) {
            promise::Promise<T, E>::reject(reason.value());
        }

        void return_value(tl::unexpected<E> &&reason) {
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

    public:
        std::suspend_never initial_suspend() {
            return {};
        };

        std::suspend_never final_suspend() noexcept {
            mFrame->next.reset();
            mFrame->location.reset();
            mFrame->cancel = nullptr;

            return {};
        }

        void unhandled_exception() {
            promise::Promise<void, std::exception_ptr>::reject(std::current_exception());
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
                const Cancellable<Result, Error> &cancellable,
                const std::source_location location = std::source_location::current()
        ) {
            mFrame->next.reset();
            mFrame->location = location;
            mFrame->cancel = cancellable.cancel;

            return {std::move(cancellable.promise)};
        }

        template<typename Result, typename Error>
        Awaitable<Result, Error> await_transform(
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
                const Task<Result, Error> &task,
                const std::source_location location = std::source_location::current()
        ) {
            mFrame->next = task.promise().mFrame;
            mFrame->location = location;
            mFrame->cancel = nullptr;

            return {task.promise()};
        }

        Task<void, std::exception_ptr> get_return_object() {
            return Task<void, std::exception_ptr>{*this};
        }

        void return_void() {
            promise::Promise<void, std::exception_ptr>::resolve();
        }

    private:
        std::shared_ptr<Frame> mFrame;

        template<typename, typename>
        friend class Task;

        template<typename, typename>
        friend class Promise;
    };

    template<typename ...Ts, typename E>
    Task<promise::promises_result_t<Ts...>, E> all(Task<Ts, E> ...tasks) {
        auto result = co_await Cancellable{
                promise::all(((promise::Promise<Ts, E>) tasks.promise())...),
                [=]() mutable {
                    std::optional<tl::expected<void, std::error_code>> result;

                    ([&]() {
                        if (result && *result)
                            return;

                        if (tasks.done())
                            return;

                        result = tasks.cancel();
                    }(), ...);

                    return *result;
                }
        };

        if (!result)
            ([&]() {
                if (tasks.done())
                    return;

                tasks.cancel();
            }(), ...);

        if constexpr (std::is_void_v<promise::promises_result_t<Ts...>> && std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            co_return;
        } else {
            co_return result;
        }
    }

    template<typename ...Ts, typename E>
    Task<std::tuple<tl::expected<Ts, E>...>, E> allSettled(Task<Ts, E> ...tasks) {
        co_return co_await Cancellable{
                promise::allSettled(((promise::Promise<Ts, E>) tasks.promise())...),
                [=]() mutable {
                    tl::expected<void, std::error_code> result;

                    ([&]() {
                        if (!result)
                            return;

                        if (tasks.done())
                            return;

                        result = tasks.cancel();
                    }(), ...);

                    return result;
                }
        };
    }

    template<
            typename ...Ts,
            typename E,
            typename F = detail::first_element_t<Ts...>,
            typename T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>
    >
    Task<T, std::list<E>> any(Task<Ts, E> ...tasks) {
        auto result = co_await Cancellable{
                promise::any(((promise::Promise<Ts, E>) tasks.promise())...),
                [=]() mutable {
                    tl::expected<void, std::error_code> result;

                    ([&]() {
                        if (!result)
                            return;

                        if (tasks.done())
                            return;

                        result = tasks.cancel();
                    }(), ...);

                    return result;
                }
        };

        if (result)
            ([&]() {
                if (tasks.done())
                    return;

                tasks.cancel();
            }(), ...);

        co_return result;
    }

    template<
            typename ...Ts,
            typename E,
            typename F = detail::first_element_t<Ts...>,
            typename T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>
    >
    Task<T, E> race(Task<Ts, E> ...tasks) {
        auto result = co_await Cancellable{
                promise::race(((promise::Promise<Ts, E>) tasks.promise())...),
                [=]() mutable {
                    std::optional<tl::expected<void, std::error_code>> result;

                    ([&]() {
                        if (result && *result)
                            return;

                        if (tasks.done())
                            return;

                        result = tasks.cancel();
                    }(), ...);

                    return *result;
                }
        };

        ([&]() {
            if (tasks.done())
                return;

            tasks.cancel();
        }(), ...);

        if constexpr (std::is_void_v<T> && std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            co_return;
        } else {
            co_return result;
        }
    }
}

#endif //ZERO_COROUTINE_H
