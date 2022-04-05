#ifndef ZERO_PROMISE_H
#define ZERO_PROMISE_H

#include <memory>
#include <functional>
#include <list>

namespace zero {
    namespace async {
        namespace promise {
            template<typename Result, typename Reason>
            class Promise;

            template<typename T>
            struct promise_traits {
                typedef T result_type;
            };

            template<typename Result, typename Reason>
            struct promise_traits<std::shared_ptr<Promise<Result, Reason>>> {
                typedef Result result_type;
                typedef Reason reason_type;
            };

            template<typename...Ts>
            using promises_result_t = decltype(std::tuple_cat(
                    std::declval<
                            typename std::conditional<
                                    std::is_same<void, typename promise_traits<Ts>::result_type>::value,
                                    std::tuple<>,
                                    std::tuple<typename promise_traits<Ts>::result_type>>::type
                    >()...)
            );

            template<typename T>
            struct function_traits : public function_traits<decltype(&T::operator())> {

            };

            template<typename T, typename Result, typename Reason, typename... Args>
            struct function_traits<std::shared_ptr<Promise<Result, Reason>>(T::*)(Args...) const> {
                typedef Result result_type;
                typedef std::function<std::shared_ptr<Promise<Result, Reason>>(
                        const std::remove_const_t<std::remove_reference_t<Args>> &...)> prototype;
            };

            template<typename T, typename ReturnType, typename... Args>
            struct function_traits<ReturnType(T::*)(Args...) const> {
                typedef ReturnType result_type;
                typedef std::function<ReturnType(
                        const std::remove_const_t<std::remove_reference_t<Args>> &...)> prototype;
            };

            template<typename T>
            struct Wrapper {
                Wrapper() = default;

                explicit Wrapper(T v) : value(std::move(v)) {

                }

                T value{};
            };

            template<>
            struct Wrapper<void> {

            };

            template<typename T>
            struct wrapper_traits {
                typedef T type;
            };

            template<typename T>
            struct wrapper_traits<Wrapper<T>> {
                typedef T type;
            };

            template<typename Result, typename... Args, typename T, size_t... I>
            auto
            invoke(const std::function<Result(Args...)> &func, const Wrapper<T> &argument, std::index_sequence<I...>) {
                return func(std::get<I>(argument.value)...);
            }

            template<typename Result, typename... Args, typename T, std::enable_if_t<std::is_same<std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...>, T>::value> * = nullptr>
            auto invoke(const std::function<Result(Args...)> &func, const Wrapper<T> &argument) {
                return invoke(func, argument, std::make_index_sequence<std::tuple_size<T>::value>{});
            }

            template<typename Result, typename Argument, typename T, std::enable_if_t<std::is_same<std::remove_const_t<std::remove_reference_t<Argument>>, T>::value> * = nullptr>
            auto invoke(const std::function<Result(Argument)> &func, const Wrapper<T> &argument) {
                return func(argument.value);
            }

            template<typename Result, typename Argument, typename T, std::enable_if_t<std::is_same<std::remove_const_t<std::remove_reference_t<Argument>>, T>::value> * = nullptr>
            auto invoke(const std::function<Result(const Wrapper<Argument> &)> &func, const Wrapper<T> &argument) {
                return func(argument);
            }

            template<typename Result, typename Argument, typename T, std::enable_if_t<
                    !std::is_same<typename wrapper_traits<std::remove_const_t<std::remove_reference_t<Argument>>>::type, T>::value &&
                    !std::is_same<std::tuple<std::remove_const_t<std::remove_reference_t<Argument>>>, T>::value> * = nullptr>
            auto invoke(const std::function<Result(Argument)> &func, const Wrapper<T> &argument) {
                return func({});
            }

            template<typename Result, typename T>
            auto invoke(const std::function<Result()> &func, const Wrapper<T> &argument) {
                return func();
            }

            template<typename P, typename Result, typename Reason, typename... Args, typename T, std::enable_if_t<!std::is_same<Result, void>::value> * = nullptr>
            auto trigger(const P &p, const std::function<std::shared_ptr<Promise<Result, Reason>>(Args...)> &func,
                         const Wrapper<T> &argument) {
                invoke(func, argument)->then([=](const Result &result) {
                    p->resolve(result);
                }, [=](const Reason &reason) {
                    p->reject(reason);
                });
            }

            template<typename P, typename Result, typename... Args, typename T, std::enable_if_t<std::is_same<Result, void>::value> * = nullptr>
            auto trigger(const P &p, const std::function<Result(Args...)> &func, const Wrapper<T> &argument) {
                invoke(func, argument);
                p->resolve();
            }

            template<typename P, typename Result, typename... Args, typename T, std::enable_if_t<!std::is_same<Result, void>::value> * = nullptr>
            auto trigger(const P &p, const std::function<Result(Args...)> &func, const Wrapper<T> &argument) {
                p->resolve(Wrapper<Result>{invoke(func, argument)});
            }

            enum emState {
                PENDING,
                FULFILLED,
                REJECTED
            };

            struct CReason {
                int code{};
                std::string message;
                std::shared_ptr<CReason> previous;
            };

            template<typename Result, typename Reason>
            class Promise : public std::enable_shared_from_this<Promise<Result, Reason>> {
            public:
                void start(const std::function<void(std::shared_ptr<Promise<Result, Reason>>)> &func) {
                    func(this->shared_from_this());
                }

            public:
                void resolve(const Wrapper<Result> &result = {}) {
                    if (mStatus != PENDING)
                        return;

                    mStatus = FULFILLED;
                    mResult = result;

                    for (const auto &n: mTriggers) {
                        std::get<0>(n)();
                    }
                }

                void reject(const Wrapper<Reason> &reason = {}) {
                    if (mStatus != PENDING)
                        return;

                    mStatus = REJECTED;
                    mReason = reason;

                    for (const auto &n: mTriggers) {
                        std::get<1>(n)();
                    }
                }

            public:
                template<typename... Args>
                void resolve(Args... args) {
                    resolve(Wrapper<Result>{{args...}});
                }

                template<typename... Args>
                void reject(Args... args) {
                    reject(Wrapper<Reason>{{args...}});
                }

            public:
                template<typename Next, typename... ResultArgs, typename... ReasonArgs>
                auto then(const std::function<Next(ResultArgs...)> &onFulfilled,
                          const std::function<Next(ReasonArgs...)> &onRejected) {
                    auto p = std::make_shared<Promise<typename promise_traits<Next>::result_type, Reason>>();

                    auto onFulfilledTrigger = [=]() {
                        if (!onFulfilled) {
                            invoke(
                                    std::function<void(const Wrapper<typename promise_traits<Next>::result_type> &)>(
                                            [p](auto &&result) {
                                                p->resolve(std::forward<decltype(result)>(result));
                                            }
                                    ),
                                    this->mResult
                            );
                            return;
                        }

                        trigger(p, onFulfilled, this->mResult);
                    };

                    auto onRejectedTrigger = [=]() {
                        if (!onRejected) {
                            invoke(
                                    std::function<void(const Wrapper<typename promise_traits<Reason>::result_type> &)>(
                                            [p](auto &&reason) {
                                                p->reject(std::forward<decltype(reason)>(reason));
                                            }
                                    ),
                                    this->mReason
                            );
                            return;
                        }

                        trigger(p, onRejected, this->mReason);
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

                auto finally(const std::function<void()> &onFinally) {
                    auto p = std::make_shared<Promise<Result, Reason>>();

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

            public:
                template<typename F>
                auto then(const F &onFulfilled) {
                    return then((typename function_traits<F>::prototype) onFulfilled, {});
                }

                template<typename F1, typename F2>
                auto then(const F1 &onFulfilled, const F2 &onRejected) {
                    return then((typename function_traits<F1>::prototype) onFulfilled,
                                (typename function_traits<F2>::prototype) onRejected);
                }

                template<typename F>
                auto fail(const F &onRejected) {
                    return then({}, (typename function_traits<F>::prototype) onRejected);
                }

                template<typename F>
                auto finally(const F &onFinally) {
                    return finally((typename function_traits<F>::prototype) onFinally);
                }

            public:
                auto status() {
                    return mStatus;
                }

                template<typename T = Result, std::enable_if_t<!std::is_same<T, void>::value> * = nullptr>
                auto value() {
                    return mResult.value;
                }

                template<typename T = Reason, std::enable_if_t<!std::is_same<T, void>::value> * = nullptr>
                auto reason() {
                    return mReason.value;
                }

            private:
                Wrapper<Result> mResult{};
                Wrapper<Reason> mReason{};

            private:
                emState mStatus{PENDING};
                std::list<std::tuple<std::function<void()>, std::function<void()>>> mTriggers;
            };

            template<typename Result>
            using CPromise = Promise<Result, CReason>;

            template<typename Result>
            auto chain(std::function<void(std::shared_ptr<CPromise<Result>>)> func) {
                auto p = std::make_shared<CPromise<Result>>();
                p->start(func);

                return p;
            }

            template<size_t index, typename P, typename R, typename Result, std::enable_if_t<!std::is_same<Result, void>::value> * = nullptr>
            void allForEach(
                    const P &p,
                    const R &results,
                    const std::shared_ptr<size_t> &remain,
                    const std::shared_ptr<CPromise<Result>> &current) {
                current->then([=](const Result &result) {
                    std::get<index>(*results) = result;

                    if (--(*remain) > 0)
                        return;

                    p->resolve(*results);
                }, [=](const CReason &reason) {
                    p->reject(reason);
                });
            }

            template<size_t index, typename P, typename R, typename Result, std::enable_if_t<std::is_same<Result, void>::value> * = nullptr>
            void allForEach(
                    const P &p,
                    const R &results,
                    const std::shared_ptr<size_t> &remain,
                    const std::shared_ptr<CPromise<Result>> &current) {
                current->then([=]() {
                    if (--(*remain) > 0)
                        return;

                    p->resolve(*results);
                }, [=](const CReason &reason) {
                    p->reject(reason);
                });
            }

            template<size_t index, typename P, typename R, typename Result, typename... Args, std::enable_if_t<!std::is_same<Result, void>::value> * = nullptr>
            void allForEach(
                    const P &p,
                    const R &results,
                    const std::shared_ptr<size_t> &remain,
                    const std::shared_ptr<CPromise<Result>> &current,
                    Args... args) {
                current->then([=](const Result &result) {
                    std::get<index>(*results) = result;

                    if (--(*remain) > 0)
                        return;

                    p->resolve(*results);
                }, [=](const CReason &reason) {
                    p->reject(reason);
                });

                allForEach<index + 1>(p, results, remain, args...);
            }

            template<size_t index, typename P, typename R, typename Result, typename... Args, std::enable_if_t<std::is_same<Result, void>::value> * = nullptr>
            void allForEach(
                    const P &p,
                    const R &results,
                    const std::shared_ptr<size_t> &remain,
                    const std::shared_ptr<CPromise<Result>> &current,
                    Args... args) {
                current->then([=]() {
                    if (--(*remain) > 0)
                        return;

                    p->resolve(*results);
                }, [=](const CReason &reason) {
                    p->reject(reason);
                });

                allForEach<index>(p, results, remain, args...);
            }

            template<typename Result, typename... Args>
            auto all(const std::shared_ptr<CPromise<Result>> &first, Args... args) {
                auto remain = std::make_shared<size_t>(sizeof...(Args) + 1);
                auto results = std::make_shared<promises_result_t<Result, Args...>>();

                return chain<promises_result_t<Result, Args...>>([=](auto p) {
                    allForEach<0>(p, results, remain, first, args...);
                });
            }

            template<size_t index, typename P, typename R, typename Result>
            void allSettledForEach(
                    const P &p,
                    const R &results,
                    const std::shared_ptr<size_t> &remain,
                    const std::shared_ptr<CPromise<Result>> &current) {
                current->finally([=]() {
                    std::get<index>(*results) = current;

                    if (--(*remain) > 0)
                        return;

                    p->resolve(*results);
                });
            }

            template<size_t index, typename P, typename R, typename Result, typename... Args>
            void allSettledForEach(
                    const P &p,
                    const R &results,
                    const std::shared_ptr<size_t> &remain,
                    const std::shared_ptr<CPromise<Result>> &current,
                    Args... args) {
                current->finally([=]() {
                    std::get<index>(*results) = current;

                    if (--(*remain) > 0)
                        return;

                    p->resolve(*results);
                });

                allSettledForEach<index + 1>(p, results, remain, args...);
            }

            template<typename Result, typename... Args>
            auto allSettled(const std::shared_ptr<CPromise<Result>> &first, Args... args) {
                auto remain = std::make_shared<size_t>(sizeof...(Args) + 1);
                auto results = std::make_shared<std::tuple<std::shared_ptr<CPromise<Result>>, Args...>>();

                return chain<std::tuple<std::shared_ptr<CPromise<Result>>, Args...>>([=](auto p) {
                    allSettledForEach<0>(p, results, remain, first, args...);
                });
            }

            template<typename... Args>
            auto any(const std::shared_ptr<CPromise<void>> &first, Args... args) {
                return chain<void>([=](auto p) {
                    auto remain = std::make_shared<size_t>(sizeof...(Args) + 1);
                    auto tail = std::make_shared<CReason>();

                    for (const auto &i: {first, args...}) {
                        i->then([=]() {
                            p->resolve();
                        }, [=](const CReason &reason) {
                            CReason last = reason;

                            if (*remain != sizeof...(Args) + 1)
                                last.previous = std::make_shared<CReason>(*tail);

                            *tail = last;

                            if (--(*remain) > 0)
                                return;

                            p->reject(*tail);
                        });
                    }
                });
            }

            template<typename Result, typename... Args>
            auto any(const std::shared_ptr<CPromise<Result>> &first, Args... args) {
                return chain<Result>([=](auto p) {
                    auto remain = std::make_shared<size_t>(sizeof...(Args) + 1);
                    auto tail = std::make_shared<CReason>();

                    for (const auto &i: {first, args...}) {
                        i->then([=](const Result &result) {
                            p->resolve(result);
                        }, [=](const CReason &reason) {
                            CReason last = reason;

                            if (*remain != sizeof...(Args) + 1)
                                last.previous = std::make_shared<CReason>(*tail);

                            *tail = last;

                            if (--(*remain) > 0)
                                return;

                            p->reject(*tail);
                        });
                    }
                });
            }

            template<typename... Args>
            auto race(const std::shared_ptr<CPromise<void>> &first, Args... args) {
                return chain<void>([=](auto p) {
                    for (const auto &i: {first, args...}) {
                        i->then([=]() {
                            p->resolve();
                        }, [=](const CReason &reason) {
                            p->reject(reason);
                        });
                    }
                });
            }

            template<typename Result, typename... Args>
            auto race(const std::shared_ptr<CPromise<Result>> &first, Args... args) {
                return chain<Result>([=](auto p) {
                    for (const auto &i: {first, args...}) {
                        i->then([=](const Result &result) {
                            p->resolve(result);
                        }, [=](const CReason &reason) {
                            p->reject(reason);
                        });
                    }
                });
            }

            template<typename Result, typename... Args>
            auto reject(Args... args) {
                auto p = std::make_shared<CPromise<Result>>();
                p->reject(args...);

                return p;
            }

            template<typename Result, typename... Args>
            auto resolve(Args... args) {
                auto p = std::make_shared<CPromise<Result>>();
                p->resolve(args...);

                return p;
            }
        }
    }
}

#endif //ZERO_PROMISE_H
