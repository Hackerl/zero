#ifndef ZERO_COROUTINE_H
#define ZERO_COROUTINE_H

#include "promise.h"
#include <coroutine>

namespace zero::async::coroutine {
    template<typename T, typename E>
    struct Awaitable {
        [[nodiscard]] bool await_ready() const {
            return promise.status() != promise::PENDING;
        }

        void await_suspend(std::coroutine_handle<> handle) {
            promise.finally([=]() {
                handle.resume();
            });
        }

        const tl::expected<T, E> &await_resume() const {
            return promise.result();
        }

        promise::Promise<T, E> promise;
    };

    template<typename T, typename E>
    struct Promise;

    template<typename T, typename E = std::exception_ptr>
    struct Task {
        using promise_type = Promise<T, E>;

        [[nodiscard]] bool done() const {
            return promise.status() != promise::PENDING;
        }

        [[nodiscard]] const tl::expected<T, E> &result() const {
            return promise.result();
        }

        Awaitable<T, E> operator co_await()  {
            return {promise};
        }

        promise::Promise<T, E> promise;
    };

    template<typename T, typename E>
    struct Promise : promise::Promise<T, E> {
        std::suspend_never initial_suspend() {
            return {};
        };

        std::suspend_never final_suspend() noexcept {
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
            return {*this};
        }

        template<typename U>
        requires (!std::is_void_v<T> && !detail::is_specialization<std::decay_t<U>, tl::expected>)
        void return_value(U &&result) {
            promise::Promise<T, E>::resolve(std::forward<U>(result));
        }

        void return_value(const tl::expected<T, E> &result) {
            return_value(tl::expected<T, E>{result});
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
    };

    template<>
    struct Promise<void, std::exception_ptr> : promise::Promise<void, std::exception_ptr> {
        std::suspend_never initial_suspend() {
            return {};
        };

        std::suspend_never final_suspend() noexcept {
            return {};
        }

        void unhandled_exception() {
            promise::Promise<void, std::exception_ptr>::reject(std::current_exception());
        }

        Task<void, std::exception_ptr> get_return_object() {
            return {*this};
        }

        void return_void() {
            promise::Promise<void, std::exception_ptr>::resolve();
        }
    };

    template<typename ...Ts, typename E>
    Task<promise::promises_result_t<Ts...>, E> all(Task<Ts, E> &...tasks) {
        using T = promise::promises_result_t<Ts...>;
        tl::expected<T, E> result = co_await promise::all(tasks.promise...);

        if constexpr (std::is_void_v<T> && std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            co_return;
        } else {
            co_return result;
        }
    }

    template<typename ...Ts>
    auto all(Ts &&...tasks) {
        return all(tasks...);
    }

    template<typename ...Ts, typename E>
    Task<std::tuple<tl::expected<Ts, E>...>, E> allSettled(Task<Ts, E> &...tasks) {
        co_return co_await promise::allSettled(tasks.promise...);
    }

    template<typename ...Ts>
    auto allSettled(Ts &&...tasks) {
        return allSettled(tasks...);
    }

    template<
            typename ...Ts,
            typename E,
            typename F = detail::first_element_t<Ts...>,
            typename T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>
    >
    Task<T, std::list<E>> any(Task<Ts, E> &...tasks) {
        co_return co_await promise::any(tasks.promise...);
    }

    template<typename ...Ts>
    auto any(Ts &&...tasks) {
        return any(tasks...);
    }

    template<
            typename ...Ts,
            typename E,
            typename F = detail::first_element_t<Ts...>,
            typename T = std::conditional_t<detail::is_elements_same_v<F, Ts...>, F, std::any>
    >
    Task<T, E> race(Task<Ts, E> &...tasks) {
        tl::expected<T, E> result = co_await promise::race(tasks.promise...);

        if constexpr (std::is_void_v<T> && std::is_same_v<E, std::exception_ptr>) {
            if (!result)
                std::rethrow_exception(result.error());

            co_return;
        } else {
            co_return result;
        }
    }

    template<typename ...Ts>
    auto race(Ts &&...tasks) {
        return race(tasks...);
    }
}

namespace zero::async::promise {
    template<typename T, typename E>
    auto operator co_await(const Promise<T, E> &promise) {
        return coroutine::Awaitable<T, E>{promise};
    }
}

#endif //ZERO_COROUTINE_H
