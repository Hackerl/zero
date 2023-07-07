#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <any>
#include <list>
#include <array>
#include <tuple>
#include <string>
#include <memory>
#include <stdexcept>
#include <functional>
#include <nonstd/expected.hpp>

#define P_CONTINUE(loop)        loop->reject({})
#define P_BREAK(loop)           loop->resolve()
#define P_BREAK_V(loop, ...)    loop->resolve(__VA_ARGS__)
#define P_BREAK_E(loop, ...)    loop->reject(__VA_ARGS__)

#define PF_RETHROW(code, message)                                                                               \
[=](const zero::async::promise::Reason &reason) {                                                               \
    return nonstd::make_unexpected(                                                                             \
        zero::async::promise::Reason{code, message, std::make_shared<zero::async::promise::Reason>(reason)}     \
    );                                                                                                          \
}

#define PF_LOOP_CONTINUE(loop)                                                                                  \
[=]() {                                                                                                         \
    P_CONTINUE(loop);                                                                                           \
}

#define PF_LOOP_THROW(loop)                                                                                     \
[=](const zero::async::promise::Reason &reason) {                                                               \
    P_BREAK_E(loop, reason);                                                                                    \
}

#define PF_LOOP_RETHROW(loop, code, message)                                                                    \
[=](const zero::async::promise::Reason &reason) {                                                               \
    P_BREAK_E(loop, {code, message, std::make_shared<zero::async::promise::Reason>(reason)});                   \
}

namespace zero::async::promise {
    struct Reason;

    template<typename Result>
    class Promise;

    template<typename T>
    struct promise_result {
        typedef T type;
    };

    template<typename Result>
    struct promise_result<std::shared_ptr<Promise<Result>>> {
        typedef Result type;
    };

    template<typename Result>
    struct promise_result<nonstd::expected<Result, Reason>> {
        typedef Result type;
    };

    template<typename T>
    using promise_result_t = typename promise_result<T>::type;

    template<typename>
    struct is_promise : std::false_type {

    };

    template<typename Result>
    struct is_promise<std::shared_ptr<Promise<Result>>> : std::true_type {

    };

    template<typename T>
    inline constexpr bool is_promise_v = is_promise<T>::value;

    template<typename, typename>
    struct is_applicable : std::false_type {

    };

    template<typename F, typename ...Ts>
    struct is_applicable<F, std::tuple<Ts...>> {
        static constexpr bool value = std::is_invocable_v<F, Ts...>;
    };

    template<typename F, typename T1, typename T2>
    struct is_applicable<F, std::pair<T1, T2>> {
        static constexpr bool value = std::is_invocable_v<F, T1, T2>;
    };

    template<typename F, typename T, size_t N>
    struct is_applicable<F, std::array<T, N>>
            : is_applicable<F, decltype(std::tuple_cat(std::declval<std::array<T, N>>()))> {

    };

    template<typename F, typename T>
    inline constexpr bool is_applicable_v = is_applicable<F, T>::value;

    template<typename T>
    struct promise_callback_traits : public promise_callback_traits<decltype(&T::operator())> {

    };

    template<typename T, typename ReturnType, typename... Args>
    struct promise_callback_traits<ReturnType(T::*)(Args...)> {
        typedef std::function<ReturnType(Args...)> type;
    };

    template<typename T, typename ReturnType, typename... Args>
    struct promise_callback_traits<ReturnType(T::*)(Args...) const> {
        typedef std::function<ReturnType(Args...)> type;
    };

    template<typename T>
    using promise_callback_t = typename promise_callback_traits<T>::type;

    enum State {
        PENDING,
        FULFILLED,
        REJECTED
    };

    struct Reason {
        int code{};
        std::string message;
        std::shared_ptr<Reason> previous;
    };

    template<typename Result>
    class Promise : public std::enable_shared_from_this<Promise<Result>> {
    public:
        template<typename F>
        void start(F &&func) {
            func(this->shared_from_this());
        }

    public:
        template<typename T>
        void resolve(T &&result) {
            if (mStatus != PENDING)
                return;

            mStatus = FULFILLED;
            mResult = std::forward<T>(result);

            for (const auto &trigger: mTriggers) {
                trigger.first();
            }
        }

        void reject(Reason &&reason) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mReason = std::move(reason);

            for (const auto &trigger: mTriggers) {
                trigger.second();
            }
        }

        void reject(const Reason &reason) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mReason = reason;

            for (const auto &trigger: mTriggers) {
                trigger.second();
            }
        }

    public:
        template<typename Next, typename ...Args, typename NextResult = std::conditional_t<std::is_same_v<Next, nonstd::unexpected_type<Reason>>, Result, promise_result_t<Next>>>
        std::shared_ptr<Promise<NextResult>>
        then(std::function<Next(Args...)> &&onFulfilled, std::function<Next(const Reason &)> &&onRejected) {
            auto p = std::make_shared<Promise<NextResult>>();

            auto onFulfilledTrigger = [=, onFulfilled = std::move(onFulfilled)]() {
                if (!onFulfilled) {
                    if constexpr (std::is_same_v<Result, NextResult>) {
                        p->resolve(mResult);
                        return;
                    } else {
                        throw std::logic_error("mismatched result type");
                    }
                }

                if constexpr (std::is_same_v<Next, void>) {
                    if constexpr (is_applicable_v<Next(Args...), Result>) {
                        std::apply(onFulfilled, mResult);
                    } else {
                        onFulfilled(mResult);
                    }

                    p->resolve();
                } else {
                    if constexpr (is_applicable_v<Next(Args...), Result>) {
                        Next next = std::apply(onFulfilled, mResult);

                        if constexpr (!is_promise_v<Next>) {
                            if constexpr (std::is_same_v<Next, nonstd::expected<NextResult, Reason>>) {
                                if (next) {
                                    if constexpr (std::is_same_v<NextResult, void>) {
                                        p->resolve();
                                    } else {
                                        p->resolve(std::move(next.value()));
                                    }
                                } else {
                                    p->reject(std::move(next.error()));
                                }
                            } else if constexpr (std::is_same_v<Next, nonstd::unexpected_type<Reason>>) {
                                p->reject(std::move(next.value()));
                            } else {
                                p->resolve(std::move(next));
                            }
                        } else {
                            if constexpr (std::is_same_v<NextResult, void>) {
                                next->then([=]() {
                                    p->resolve();
                                }, [=](const Reason &reason) {
                                    p->reject(reason);
                                });
                            } else {
                                next->then([=](const NextResult &result) {
                                    p->resolve(result);
                                }, [=](const Reason &reason) {
                                    p->reject(reason);
                                });
                            }
                        }
                    } else {
                        Next next = onFulfilled(mResult);

                        if constexpr (!is_promise_v<Next>) {
                            if constexpr (std::is_same_v<Next, nonstd::expected<NextResult, Reason>>) {
                                if (next) {
                                    if constexpr (std::is_same_v<NextResult, void>) {
                                        p->resolve();
                                    } else {
                                        p->resolve(std::move(next.value()));
                                    }
                                } else {
                                    p->reject(std::move(next.error()));
                                }
                            } else if constexpr (std::is_same_v<Next, nonstd::unexpected_type<Reason>>) {
                                p->reject(std::move(next.value()));
                            } else {
                                p->resolve(std::move(next));
                            }
                        } else {
                            if constexpr (std::is_same_v<NextResult, void>) {
                                next->then([=]() {
                                    p->resolve();
                                }, [=](const Reason &reason) {
                                    p->reject(reason);
                                });
                            } else {
                                next->then([=](const NextResult &result) {
                                    p->resolve(result);
                                }, [=](const Reason &reason) {
                                    p->reject(reason);
                                });
                            }
                        }
                    }
                }
            };

            auto onRejectedTrigger = [=, onRejected = std::move(onRejected)]() {
                if (!onRejected) {
                    p->reject(mReason);
                    return;
                }

                if constexpr (std::is_same_v<Next, void>) {
                    onRejected(mReason);
                    p->resolve();
                } else {
                    Next next = onRejected(mReason);

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (std::is_same_v<Next, nonstd::expected<NextResult, Reason>>) {
                            if (next) {
                                if constexpr (std::is_same_v<NextResult, void>) {
                                    p->resolve();
                                } else {
                                    p->resolve(std::move(next.value()));
                                }
                            } else {
                                p->reject(std::move(next.error()));
                            }
                        } else if constexpr (std::is_same_v<Next, nonstd::unexpected_type<Reason>>) {
                            p->reject(std::move(next.value()));
                        } else {
                            p->resolve(std::move(next));
                        }
                    } else {
                        if constexpr (std::is_same_v<NextResult, void>) {
                            next->then([=]() {
                                p->resolve();
                            }, [=](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([=](const NextResult &result) {
                                p->resolve(result);
                            }, [=](const Reason &reason) {
                                p->reject(reason);
                            });
                        }
                    }
                }
            };

            switch (mStatus) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mTriggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
            }

            return p;
        }

        std::shared_ptr<Promise<Result>> finally(const std::function<void()> &onFinally) {
            auto p = std::make_shared<Promise<Result>>();

            auto onFulfilledTrigger = [=]() {
                onFinally();
                p->resolve(mResult);
            };

            auto onRejectedTrigger = [=]() {
                onFinally();
                p->reject(mReason);
            };

            switch (mStatus) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mTriggers.push_back({std::move(onFulfilledTrigger), std::move(onRejectedTrigger)});
            }

            return p;
        }

        template<typename F>
        auto then(F &&onFulfilled) {
            return then(promise_callback_t<F>{std::forward<F>(onFulfilled)}, {});
        }

        template<typename F, typename R>
        auto then(F &&onFulfilled, R &&onRejected) {
            return then(
                    promise_callback_t<F>{std::forward<F>(onFulfilled)},
                    promise_callback_t<R>{std::forward<R>(onRejected)}
            );
        }

        template<typename R>
        auto fail(R &&onRejected) {
            return then(
                    std::function<std::invoke_result_t<R, Reason>(Result)>{},
                    promise_callback_t<R>{std::forward<R>(onRejected)}
            );
        }

    public:
        [[nodiscard]] State status() const {
            return mStatus;
        }

        [[nodiscard]] Result value() const {
            return mResult;
        }

        [[nodiscard]] Reason reason() const {
            return mReason;
        }

    private:
        Result mResult;
        Reason mReason;

    private:
        State mStatus{PENDING};
        std::list<std::pair<std::function<void()>, std::function<void()>>> mTriggers;
    };

    template<>
    class Promise<void> : public std::enable_shared_from_this<Promise<void>> {
    public:
        template<typename F>
        void start(F &&func) {
            func(shared_from_this());
        }

    public:
        void resolve() {
            if (mStatus != PENDING)
                return;

            mStatus = FULFILLED;

            for (const auto &trigger: mTriggers) {
                trigger.first();
            }
        }

        void reject(Reason &&reason) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mReason = std::move(reason);

            for (const auto &trigger: mTriggers) {
                trigger.second();
            }
        }

        void reject(const Reason &reason) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mReason = reason;

            for (const auto &trigger: mTriggers) {
                trigger.second();
            }
        }

    public:
        template<typename Next, typename NextResult = std::conditional_t<std::is_same_v<Next, nonstd::unexpected_type<Reason>>, void, promise_result_t<Next>>>
        std::shared_ptr<Promise<NextResult>>
        then(std::function<Next()> &&onFulfilled, std::function<Next(const Reason &)> &&onRejected) {
            auto p = std::make_shared<Promise<NextResult>>();

            auto onFulfilledTrigger = [=, onFulfilled = std::move(onFulfilled)]() {
                if (!onFulfilled) {
                    if constexpr (std::is_same_v<NextResult, void>) {
                        p->resolve();
                        return;
                    } else {
                        throw std::logic_error("mismatched result type");
                    }
                }

                if constexpr (std::is_same_v<Next, void>) {
                    onFulfilled();
                    p->resolve();
                } else {
                    Next next = onFulfilled();

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (std::is_same_v<Next, nonstd::expected<NextResult, Reason>>) {
                            if (next) {
                                if constexpr (std::is_same_v<NextResult, void>) {
                                    p->resolve();
                                } else {
                                    p->resolve(std::move(next.value()));
                                }
                            } else {
                                p->reject(std::move(next.error()));
                            }
                        } else if constexpr (std::is_same_v<Next, nonstd::unexpected_type<Reason>>) {
                            p->reject(std::move(next.value()));
                        } else {
                            p->resolve(std::move(next));
                        }
                    } else {
                        if constexpr (std::is_same_v<NextResult, void>) {
                            next->then([=]() {
                                p->resolve();
                            }, [=](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([=](const NextResult &result) {
                                p->resolve(result);
                            }, [=](const Reason &reason) {
                                p->reject(reason);
                            });
                        }
                    }
                }
            };

            auto onRejectedTrigger = [=, onRejected = std::move(onRejected)]() {
                if (!onRejected) {
                    p->reject(mReason);
                    return;
                }

                if constexpr (std::is_same_v<Next, void>) {
                    onRejected(mReason);
                    p->resolve();
                } else {
                    Next next = onRejected(mReason);

                    if constexpr (!is_promise_v<Next>) {
                        if constexpr (std::is_same_v<Next, nonstd::expected<NextResult, Reason>>) {
                            if (next) {
                                if constexpr (std::is_same_v<NextResult, void>) {
                                    p->resolve();
                                } else {
                                    p->resolve(std::move(next.value()));
                                }
                            } else {
                                p->reject(std::move(next.error()));
                            }
                        } else if constexpr (std::is_same_v<Next, nonstd::unexpected_type<Reason>>) {
                            p->reject(std::move(next.value()));
                        } else {
                            p->resolve(std::move(next));
                        }
                    } else {
                        if constexpr (std::is_same_v<NextResult, void>) {
                            next->then([=]() {
                                p->resolve();
                            }, [=](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([=](const NextResult &result) {
                                p->resolve(result);
                            }, [=](const Reason &reason) {
                                p->reject(reason);
                            });
                        }
                    }
                }
            };

            switch (mStatus) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mTriggers.push_back({std::move(onFulfilledTrigger), std::move(onRejectedTrigger)});
            }

            return p;
        }

        std::shared_ptr<Promise<void>> finally(const std::function<void()> &onFinally) {
            auto p = std::make_shared<Promise<void>>();

            auto onFulfilledTrigger = [=]() {
                onFinally();
                p->resolve();
            };

            auto onRejectedTrigger = [=]() {
                onFinally();
                p->reject(mReason);
            };

            switch (mStatus) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mTriggers.emplace_back(std::move(onFulfilledTrigger), std::move(onRejectedTrigger));
            }

            return p;
        }

        template<typename F>
        auto then(F &&onFulfilled) {
            return then(promise_callback_t<F>{std::forward<F>(onFulfilled)}, {});
        }

        template<typename F, typename R>
        auto then(F &&onFulfilled, R &&onRejected) {
            return then(
                    promise_callback_t<F>{std::forward<F>(onFulfilled)},
                    promise_callback_t<R>{std::forward<R>(onRejected)}
            );
        }

        template<typename R>
        auto fail(R &&onRejected) {
            return then({}, promise_callback_t<R>{std::forward<R>(onRejected)});
        }

    public:
        [[nodiscard]] State status() const {
            return mStatus;
        }

        [[nodiscard]] Reason reason() const {
            return mReason;
        }

    private:
        Reason mReason;

    private:
        State mStatus{PENDING};
        std::list<std::pair<std::function<void()>, std::function<void()>>> mTriggers;
    };

    template<typename Result, typename F>
    std::shared_ptr<Promise<Result>> chain(F &&func) {
        auto p = std::make_shared<Promise<Result>>();
        p->start(std::forward<F>(func));

        return p;
    }

    template<typename Result>
    std::shared_ptr<Promise<Result>> reject(Reason &&reason) {
        auto p = std::make_shared<Promise<Result>>();
        p->reject(std::move(reason));

        return p;
    }

    template<typename Result>
    std::shared_ptr<Promise<Result>> reject(const Reason &reason) {
        auto p = std::make_shared<Promise<Result>>();
        p->reject(reason);

        return p;
    }

    template<typename Result, typename... Ts>
    std::shared_ptr<Promise<Result>> resolve(Ts &&... args) {
        auto p = std::make_shared<Promise<Result>>();
        p->resolve(std::forward<Ts>(args)...);

        return p;
    }

    template<typename...Ts>
    using first_element_t = std::tuple_element_t<0, std::tuple<Ts...>>;

    template<typename T, typename...Ts>
    inline constexpr bool is_elements_same_v = (std::is_same_v<T, Ts> && ...);

    template<typename...Ts>
    using promises_result_t = std::conditional_t<
            is_elements_same_v<first_element_t<Ts...>, Ts...>,
            std::conditional_t<
                    std::is_same_v<first_element_t<Ts...>, void>,
                    void,
                    std::array<first_element_t<Ts...>, sizeof...(Ts)>
            >,
            decltype(std::tuple_cat(
                    std::declval<
                            std::conditional_t<
                                    std::is_same_v<Ts, void>,
                                    std::tuple<>,
                                    std::tuple<Ts>
                            >
                    >()...)
            )
    >;

    template<size_t Count, size_t Index, size_t N, size_t... Is>
    struct promises_result_index_sequence : promises_result_index_sequence<Count - 1, Index + N, Is..., Index> {

    };

    template<size_t Index, size_t N, size_t... Is>
    struct promises_result_index_sequence<0, Index, N, Is...> : std::index_sequence<N, Is...> {

    };

    template<typename...Ts>
    using promises_result_index_sequence_for = promises_result_index_sequence<
            sizeof...(Ts),
            0,
            (std::is_same_v<Ts, void> ? 0 : 1)...
    >;

    template<size_t...Index, typename ...Results>
    std::shared_ptr<Promise<promises_result_t<Results...>>>
    all(std::index_sequence<Index...>, const std::shared_ptr<Promise<Results>> &... promises) {
        auto remain = std::make_shared<size_t>(sizeof...(promises));
        auto p = std::make_shared<Promise<promises_result_t<Results...>>>();

        if constexpr (std::is_same_v<promises_result_t<Results...>, void>) {
            ([=]() {
                promises->then([=]() {
                    if (--(*remain) > 0)
                        return;

                    p->resolve();
                }, [=](const Reason &reason) {
                    p->reject(reason);
                });
            }(), ...);
        } else {
            auto results = std::make_shared<promises_result_t<Results...>>();

            ([=]() {
                if constexpr (std::is_same_v<Results, void>) {
                    promises->then([=]() {
                        if (--(*remain) > 0)
                            return;

                        p->resolve(std::move(*results));
                    }, [=](const Reason &reason) {
                        p->reject(reason);
                    });
                } else {
                    promises->then([=](const Results &result) {
                        std::get<Index>(*results) = result;

                        if (--(*remain) > 0)
                            return;

                        p->resolve(std::move(*results));
                    }, [=](const Reason &reason) {
                        p->reject(reason);
                    });
                }
            }(), ...);
        }

        return p;
    }

    template<typename ...Results>
    std::shared_ptr<Promise<promises_result_t<Results...>>> all(const std::shared_ptr<Promise<Results>> &... promises) {
        return all(promises_result_index_sequence_for<Results...>{}, promises...);
    }

    template<size_t... Index, typename ...Results>
    std::shared_ptr<Promise<std::tuple<std::shared_ptr<Promise<Results>>...>>>
    allSettled(std::index_sequence<Index...>, const std::shared_ptr<Promise<Results>> &... promises) {
        auto remain = std::make_shared<size_t>(sizeof...(Results));
        auto results = std::make_shared<std::tuple<std::shared_ptr<Promise<Results>>...>>();
        auto p = std::make_shared<Promise<std::tuple<std::shared_ptr<Promise<Results>>...>>>();

        ([=]() {
            promises->finally([=]() {
                std::get<Index>(*results) = promises;

                if (--(*remain) > 0)
                    return;

                p->resolve(std::move(*results));
            });
        }(), ...);

        return p;
    }

    template<typename ...Results>
    std::shared_ptr<Promise<std::tuple<std::shared_ptr<Promise<Results>>...>>>
    allSettled(const std::shared_ptr<Promise<Results>> &... promises) {
        return allSettled(std::index_sequence_for<Results...>{}, promises...);
    }

    template<typename ...Results, typename T = first_element_t<Results...>, bool Same = is_elements_same_v<T, Results...>>
    std::shared_ptr<Promise<std::conditional_t<Same, T, std::any>>>
    any(const std::shared_ptr<Promise<Results>> &... promises) {
        auto remain = std::make_shared<size_t>(sizeof...(Results));
        auto tail = std::make_shared<Reason>();
        auto p = std::make_shared<Promise<std::conditional_t<Same, T, std::any>>>();

        ([=]() {
            if constexpr (std::is_same_v<Results, void>) {
                promises->then([=]() {
                    if constexpr (Same)
                        p->resolve();
                    else
                        p->resolve(std::any{});
                }, [=](const Reason &reason) {
                    Reason last = reason;

                    if (*remain != sizeof...(Results))
                        last.previous = std::make_shared<Reason>(*tail);

                    *tail = last;

                    if (--(*remain) > 0)
                        return;

                    p->reject(std::move(*tail));
                });
            } else {
                promises->then([=](const Results &result) {
                    p->resolve(result);
                }, [=](const Reason &reason) {
                    Reason last = reason;

                    if (*remain != sizeof...(Results))
                        last.previous = std::make_shared<Reason>(*tail);

                    *tail = last;

                    if (--(*remain) > 0)
                        return;

                    p->reject(std::move(*tail));
                });
            }
        }(), ...);

        return p;
    }

    template<typename ...Results, typename T = first_element_t<Results...>, bool Same = is_elements_same_v<T, Results...>>
    std::shared_ptr<Promise<std::conditional_t<Same, T, std::any>>>
    race(const std::shared_ptr<Promise<Results>> &... promises) {
        auto p = std::make_shared<Promise<std::conditional_t<Same, T, std::any>>>();

        ([=]() {
            if constexpr (std::is_same_v<Results, void>) {
                promises->then([=]() {
                    if constexpr (Same)
                        p->resolve();
                    else
                        p->resolve(std::any{});
                }, [=](const Reason &reason) {
                    p->reject(reason);
                });
            } else {
                promises->then([=](const Results &result) {
                    p->resolve(result);
                }, [=](const Reason &reason) {
                    p->reject(reason);
                });
            }
        }(), ...);

        return p;
    }

    template<typename Result, typename F>
    void repeat(
            const std::shared_ptr<Promise<Result>> &loop,
            F &&func
    ) {
        std::shared_ptr<Promise<Result>> p = chain<Result>(func);

        while (p->status() == REJECTED && p->reason().code == 0 && p->reason().message.empty())
            p = chain<Result>(func);

        if constexpr (std::is_same_v<Result, void>) {
            p->then([=]() {
                loop->resolve();
            }, [=, func = std::forward<F>(func)](const Reason &reason) mutable {
                if (reason.code == 0 && reason.message.empty()) {
                    repeat(loop, std::move(func));
                    return;
                }

                loop->reject(reason);
            });
        } else {
            p->then([=](const Result &result) {
                loop->resolve(result);
            }, [=, func = std::forward<F>(func)](const Reason &reason) mutable {
                if (reason.code == 0 && reason.message.empty()) {
                    repeat(loop, std::move(func));
                    return;
                }

                loop->reject(reason);
            });
        }
    }

    template<typename Result, typename F>
    std::shared_ptr<Promise<Result>> loop(F &&func) {
        return chain<Result>([func = std::forward<F>(func)](const auto &loop) mutable {
            repeat<Result>(loop, std::move(func));
        });
    }

    template<typename F>
    std::shared_ptr<Promise<void>> doWhile(F &&func) {
        return loop<void>([f = std::forward<F>(func)](const auto &loop) {
            f()->then(
                    PF_LOOP_CONTINUE(loop),
                    PF_LOOP_THROW(loop)
            );
        });
    }
}

#endif //ZERO_PROMISE_H
