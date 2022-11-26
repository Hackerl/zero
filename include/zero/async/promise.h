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

#define P_CONTINUE(p)       p->reject({})
#define P_BREAK(p)          p->resolve()
#define P_BREAK_V(p, ...)   p->resolve(__VA_ARGS__)
#define P_BREAK_E(p, ...)   p->reject(__VA_ARGS__)

namespace zero::async::promise {
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

    template<typename T>
    struct promise_function : public promise_function<decltype(&T::operator())> {

    };

    template<typename T, typename ReturnType, typename... Args>
    struct promise_function<ReturnType(T::*)(Args...) const> {
        typedef std::function<ReturnType(const std::remove_const_t<std::remove_reference_t<Args>> &...)> type;
    };

    template<typename T>
    using promise_function_t = typename promise_function<T>::type;

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
        void start(const std::function<void(std::shared_ptr<Promise<Result>>)> &func) {
            func(this->shared_from_this());
        }

    public:
        template<typename T>
        void resolve(const T &result) {
            if (mStatus != PENDING)
                return;

            mStatus = FULFILLED;
            mResult = result;

            for (const auto &n: mTriggers) {
                n.first();
            }
        }

        void reject(const Reason &reason) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mReason = reason;

            for (const auto &n: mTriggers) {
                n.second();
            }
        }

    public:
        template<typename Next, typename ...Args, typename NextResult = promise_result_t<Next>>
        std::shared_ptr<Promise<NextResult>>
        then(const std::function<Next(Args...)> &onFulfilled, const std::function<Next(const Reason &)> &onRejected) {
            auto p = std::make_shared<Promise<NextResult>>();

            auto onFulfilledTrigger = [=]() {
                if (!onFulfilled) {
                    if constexpr (std::is_same_v<Result, NextResult>) {
                        p->resolve(this->mResult);
                        return;
                    } else {
                        throw std::logic_error("mismatched result type");
                    }
                }

                if constexpr (std::is_same_v<Next, void>) {
                    if constexpr (std::is_same_v<Result, std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>>) {
                        std::apply(onFulfilled, this->mResult);
                    } else {
                        onFulfilled(this->mResult);
                    }

                    p->resolve();
                } else {
                    Next next;

                    if constexpr (std::is_same_v<Result, std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>>) {
                        next = std::apply(onFulfilled, this->mResult);
                    } else {
                        next = onFulfilled(this->mResult);
                    }

                    if constexpr (!is_promise_v<Next>) {
                        p->resolve(next);
                    } else {
                        if constexpr (std::is_same_v<void, NextResult>) {
                            next->then([p]() {
                                p->resolve();
                            }, [p](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([p](const NextResult &result) {
                                p->resolve(result);
                            }, [p](const Reason &reason) {
                                p->reject(reason);
                            });
                        }
                    }
                }
            };

            auto onRejectedTrigger = [=]() {
                if (!onRejected) {
                    p->reject(this->mReason);
                    return;
                }

                if constexpr (std::is_same_v<Next, void>) {
                    onRejected(this->mReason);
                    p->resolve();
                } else {
                    Next next = onRejected(this->mReason);

                    if constexpr (!is_promise_v<Next>) {
                        p->resolve(next);
                    } else {
                        if constexpr (std::is_same_v<void, NextResult>) {
                            next->then([p]() {
                                p->resolve();
                            }, [p](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([p](const NextResult &result) {
                                p->resolve(result);
                            }, [p](const Reason &reason) {
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
                    mTriggers.emplace_back(onFulfilledTrigger, onRejectedTrigger);
            }

            return p;
        }

        std::shared_ptr<Promise<Result>> finally(const std::function<void()> &onFinally) {
            auto p = std::make_shared<Promise<Result>>();

            auto onFulfilledTrigger = [=]() {
                onFinally();
                p->resolve(this->mResult);
            };

            auto onRejectedTrigger = [=]() {
                onFinally();
                p->reject(this->mReason);
            };

            switch (mStatus) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mTriggers.push_back({onFulfilledTrigger, onRejectedTrigger});
            }

            return p;
        }

        template<typename F>
        auto then(const F &onFulfilled) {
            return then((promise_function_t<F>) onFulfilled, {});
        }

        template<typename F, typename R>
        auto then(const F &onFulfilled, const R &onRejected) {
            return then((promise_function_t<F>) onFulfilled, (promise_function_t<R>) onRejected);
        }

        template<typename F, typename Next = std::invoke_result_t<F, Reason>>
        auto fail(const F &onRejected) {
            return then(std::function<Next(Result)>{}, (promise_function_t<F>) onRejected);
        }

    public:
        State status() {
            return mStatus;
        }

        Result value() {
            return mResult;
        }

        Reason reason() {
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
        void start(const std::function<void(std::shared_ptr<Promise<void>>)> &func) {
            func(this->shared_from_this());
        }

    public:
        void resolve() {
            if (mStatus != PENDING)
                return;

            mStatus = FULFILLED;

            for (const auto &n: mTriggers) {
                n.first();
            }
        }

        void reject(const Reason &reason) {
            if (mStatus != PENDING)
                return;

            mStatus = REJECTED;
            mReason = reason;

            for (const auto &n: mTriggers) {
                n.second();
            }
        }

    public:
        template<typename Next, typename NextResult = promise_result_t<Next>>
        std::shared_ptr<Promise<NextResult>>
        then(const std::function<Next()> &onFulfilled, const std::function<Next(const Reason &)> &onRejected) {
            auto p = std::make_shared<Promise<NextResult>>();

            auto onFulfilledTrigger = [=]() {
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
                        p->resolve(next);
                    } else {
                        if constexpr (std::is_same_v<NextResult, void>) {
                            next->then([p]() {
                                p->resolve();
                            }, [p](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([p](const NextResult &result) {
                                p->resolve(result);
                            }, [p](const Reason &reason) {
                                p->reject(reason);
                            });
                        }
                    }
                }
            };

            auto onRejectedTrigger = [=]() {
                if (!onRejected) {
                    p->reject(this->mReason);
                    return;
                }

                if constexpr (std::is_same_v<Next, void>) {
                    onRejected(this->mReason);
                    p->resolve();
                } else {
                    Next next = onRejected(this->mReason);

                    if constexpr (!is_promise_v<Next>) {
                        p->resolve(next);
                    } else {
                        if constexpr (std::is_same_v<NextResult, void>) {
                            next->then([p]() {
                                p->resolve();
                            }, [p](const Reason &reason) {
                                p->reject(reason);
                            });
                        } else {
                            next->then([p](const NextResult &result) {
                                p->resolve(result);
                            }, [p](const Reason &reason) {
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
                    mTriggers.push_back({onFulfilledTrigger, onRejectedTrigger});
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
                p->reject(this->mReason);
            };

            switch (mStatus) {
                case FULFILLED:
                    onFulfilledTrigger();
                    break;

                case REJECTED:
                    onRejectedTrigger();
                    break;

                case PENDING:
                    mTriggers.emplace_back(onFulfilledTrigger, onRejectedTrigger);
            }

            return p;
        }

        template<typename F>
        auto then(const F &onFulfilled) {
            return then((promise_function_t<F>) onFulfilled, {});
        }

        template<typename F, typename R>
        auto then(const F &onFulfilled, const R &onRejected) {
            return then((promise_function_t<F>) onFulfilled, (promise_function_t<R>) onRejected);
        }

        template<typename F, typename Next = std::invoke_result_t<F, Reason>>
        auto fail(const F &onRejected) {
            return then(std::function<Next()>{}, (promise_function_t<F>) onRejected);
        }

    public:
        State status() {
            return mStatus;
        }

        Reason reason() {
            return mReason;
        }

    private:
        Reason mReason;

    private:
        State mStatus{PENDING};
        std::list<std::pair<std::function<void()>, std::function<void()>>> mTriggers;
    };

    template<typename Result>
    std::shared_ptr<Promise<Result>> chain(const std::function<void(std::shared_ptr<Promise<Result>>)> &func) {
        auto p = std::make_shared<Promise<Result>>();
        p->start(func);

        return p;
    }

    template<typename Result>
    std::shared_ptr<Promise<Result>> reject(const Reason &reason) {
        auto p = std::make_shared<Promise<Result>>();
        p->reject(reason);

        return p;
    }

    template<typename Result, typename... Ts>
    std::shared_ptr<Promise<Result>> resolve(const Ts &... args) {
        auto p = std::make_shared<Promise<Result>>();
        p->resolve(args...);

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

                        p->resolve(*results);
                    }, [=](const Reason &reason) {
                        p->reject(reason);
                    });
                } else {
                    promises->then([=](const Results &result) {
                        std::get<Index>(*results) = result;

                        if (--(*remain) > 0)
                            return;

                        p->resolve(*results);
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

                p->resolve(*results);
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

                    p->reject(*tail);
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

                    p->reject(*tail);
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

    template<typename Result>
    void repeat(const std::shared_ptr<Promise<Result>> &p, const std::function<void(std::shared_ptr<Promise<Result>>)> &func) {
        if constexpr (std::is_same_v<Result, void>) {
            chain<Result>(func)->then([=]() {
                p->resolve();
            }, [=](const Reason &reason) {
                if (reason.code < 0) {
                    p->reject(reason);
                    return;
                }

                repeat(p, func);
            });
        } else {
            chain<Result>(func)->then([=](const Result &result) {
                p->resolve(result);
            }, [=](const Reason &reason) {
                if (reason.code < 0) {
                    p->reject(reason);
                    return;
                }

                repeat(p, func);
            });
        }
    }

    template<typename Result>
    std::shared_ptr<Promise<Result>> loop(const std::function<void(std::shared_ptr<Promise<Result>>)> &func) {
        return chain<Result>([=](const auto &p) {
            repeat(p, func);
        });
    }
}

#endif //ZERO_PROMISE_H
