#ifndef ZERO_META_TYPE_TRAITS_H
#define ZERO_META_TYPE_TRAITS_H

#include <tuple>
#include <type_traits>

namespace zero::meta {
    template<typename, template<typename...> class>
    inline constexpr bool IsSpecialization = false;

    template<template<typename...> class Template, typename... Ts>
    inline constexpr bool IsSpecialization<Template<Ts...>, Template> = true;

    template<typename T, typename... Ts>
    inline constexpr bool IsAllSame = (std::is_same_v<T, Ts> && ...);

    template<std::size_t I, typename... Ts>
    using Element = std::tuple_element_t<I, std::tuple<Ts...>>;

    template<typename... Ts>
    using FirstElement = Element<0, Ts...>;

    template<typename F, typename = void>
    struct FunctionTraits;

    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...), void> : FunctionTraits<R(Args...)> {
    };

    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...) noexcept, void> : FunctionTraits<R(Args...)> {
    };

    template<typename R, typename... Args>
    struct FunctionTraits<R(Args...), void> {
        using ReturnType = R;

        static constexpr auto Arity = sizeof...(Args);

        template<std::size_t N>
        struct Argument {
            static_assert(N < Arity, "Invalid parameter index");
            using Type = Element<N, Args...>;
        };
    };

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...), void> : FunctionTraits<R(C &, Args...)> {
    };

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) noexcept, void> : FunctionTraits<R(C &, Args...)> {
    };

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) const, void> : FunctionTraits<R(C &, Args...)> {
    };

    template<typename C, typename R, typename... Args>
    struct FunctionTraits<R(C::*)(Args...) const noexcept, void> : FunctionTraits<R(C &, Args...)> {
    };

    template<typename C, typename R>
    struct FunctionTraits<R(C::*), void> : FunctionTraits<R(C &)> {
    };

    template<typename F>
        requires requires { &F::operator(); }
    struct FunctionTraits<F, void> {
    private:
        using CallType = FunctionTraits<decltype(&F::operator())>;

    public:
        using ReturnType = CallType::ReturnType;

        static constexpr auto Arity = CallType::Arity - 1;

        template<std::size_t N>
        struct Argument {
            static_assert(N < Arity, "Invalid parameter index");
            using Type = CallType::template Argument<N + 1>::Type;
        };
    };

    template<typename F>
    struct FunctionTraits<F &, void> : FunctionTraits<std::remove_cv_t<F>> {
    };

    template<typename F>
    struct FunctionTraits<F &&, void> : FunctionTraits<std::remove_cv_t<F>> {
    };

    template<typename F, std::size_t N, typename... Ts>
    struct FunctionArgumentsImpl :
        FunctionArgumentsImpl<F, N - 1, typename FunctionTraits<F>::template Argument<N>::Type, Ts...> {
    };

    template<typename F, typename... Ts>
    struct FunctionArgumentsImpl<F, 0, Ts...> {
        using Type = std::tuple<typename FunctionTraits<F>::template Argument<0>::Type, Ts...>;
    };

    template<typename F>
    using FunctionResult = FunctionTraits<F>::ReturnType;

    template<typename F>
    using FunctionArguments = FunctionArgumentsImpl<F, FunctionTraits<F>::Arity - 1>::Type;
}

#endif //ZERO_META_TYPE_TRAITS_H
