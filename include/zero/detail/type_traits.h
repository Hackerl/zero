#ifndef ZERO_TYPE_TRAITS_H
#define ZERO_TYPE_TRAITS_H

#include <array>
#include <tuple>
#include <type_traits>

namespace zero::detail {
    template<typename, template<typename...> class>
    inline constexpr bool is_specialization_v = false;

    template<template<typename...> class T, typename... Args>
    inline constexpr bool is_specialization_v<T<Args...>, T> = true;

    template<typename F, typename T>
    inline constexpr bool is_applicable_v = false;

    template<typename F, typename... Ts>
    inline constexpr bool is_applicable_v<F, std::tuple<Ts...>> = std::is_invocable_v<F, Ts...>;

    template<typename F, typename T1, typename T2>
    inline constexpr bool is_applicable_v<F, std::pair<T1, T2>> = std::is_invocable_v<F, T1, T2>;

    template<typename F, typename T, std::size_t N>
    inline constexpr bool is_applicable_v<F, std::array<T, N>> = is_applicable_v<
        F,
        decltype(std::tuple_cat(std::declval<std::array<T, N>>()))
    >;

    template<std::size_t I, typename... Ts>
    using element_t = std::tuple_element_t<I, std::tuple<Ts...>>;

    template<typename... Ts>
    using first_element_t = element_t<0, Ts...>;

    template<typename T, typename... Ts>
    inline constexpr bool all_same_v = (std::is_same_v<T, Ts> && ...);

    template<typename F>
    struct function_traits;

    template<typename R, typename... Args>
    struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {
    };

    template<typename R, typename... Args>
    struct function_traits<R(*)(Args...) noexcept> : function_traits<R(Args...)> {
    };

    template<typename R, typename... Args>
    struct function_traits<R(Args...)> {
        using return_type = R;

        static constexpr std::size_t arity = sizeof...(Args);

        template<std::size_t N>
        struct argument {
            static_assert(N < arity, "invalid parameter index");
            using type = element_t<N, Args...>;
        };
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...)> : function_traits<R(C &, Args...)> {
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) noexcept> : function_traits<R(C &, Args...)> {
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) const> : function_traits<R(C &, Args...)> {
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) const noexcept> : function_traits<R(C &, Args...)> {
    };

    template<typename C, typename R>
    struct function_traits<R(C::*)> : function_traits<R(C &)> {
    };

    template<typename F>
    struct function_traits {
    private:
        using call_type = function_traits<decltype(&F::operator())>;

    public:
        using return_type = typename call_type::return_type;

        static constexpr std::size_t arity = call_type::arity - 1;

        template<std::size_t N>
        struct argument {
            static_assert(N < arity, "invalid parameter index");
            using type = typename call_type::template argument<N + 1>::type;
        };
    };

    template<typename F>
    struct function_traits<F &> : function_traits<F> {
    };

    template<typename F>
    struct function_traits<F &&> : function_traits<F> {
    };

    template<typename F, std::size_t N, typename... Ts>
    struct function_arguments :
        function_arguments<F, N - 1, typename function_traits<F>::template argument<N>::type, Ts...> {
    };

    template<typename F, typename... Ts>
    struct function_arguments<F, 0, Ts...> {
        using type = std::tuple<typename function_traits<F>::template argument<0>::type, Ts...>;
    };

    template<typename F>
    using function_result_t = typename function_traits<F>::return_type;

    template<typename F>
    using function_arguments_t = typename function_arguments<F, function_traits<F>::arity - 1>::type;
}

#endif //ZERO_TYPE_TRAITS_H
