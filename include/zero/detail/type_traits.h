#ifndef ZERO_TYPE_TRAITS_H
#define ZERO_TYPE_TRAITS_H

#include <list>
#include <array>
#include <tuple>
#include <vector>
#include <functional>
#include <type_traits>

namespace zero::detail {
    template<typename, template<typename...> class>
    inline constexpr bool is_specialization = false;

    template<template<typename...> class T, typename... Args>
    inline constexpr bool is_specialization<T<Args...>, T> = true;

    template<typename T>
    concept Vector = is_specialization<T, std::vector>;

    template<typename T>
    concept List = is_specialization<T, std::list>;

    template<typename T>
    concept Tuple = is_specialization<T, std::tuple>;

    template<typename T>
    concept Pair = is_specialization<T, std::pair>;

    template<typename, typename>
    struct is_applicable : std::false_type {
    };

    template<typename F, typename... Ts>
    struct is_applicable<F, std::tuple<Ts...>> {
        static constexpr bool value = std::is_invocable_v<F, Ts...>;
    };

    template<typename F, typename T1, typename T2>
    struct is_applicable<F, std::pair<T1, T2>> {
        static constexpr bool value = std::is_invocable_v<F, T1, T2>;
    };

    template<typename F, typename T, std::size_t N>
    struct is_applicable<F, std::array<T, N>>
        : is_applicable<F, decltype(std::tuple_cat(std::declval<std::array<T, N>>()))> {
    };

    template<typename F, typename T>
    inline constexpr bool is_applicable_v = is_applicable<F, T>::value;

    template<typename T>
    struct callable_traits : callable_traits<decltype(&T::operator())> {
    };

    template<typename T, typename ReturnType, typename... Args>
    struct callable_traits<ReturnType(T::*)(Args...)> {
        using result = ReturnType;
        using arguments = std::tuple<Args...>;
        using type = std::function<ReturnType(Args...)>;
    };

    template<typename T, typename ReturnType, typename... Args>
    struct callable_traits<ReturnType(T::*)(Args...) const> {
        using result = ReturnType;
        using arguments = std::tuple<Args...>;
        using type = std::function<ReturnType(Args...)>;
    };

    template<typename T>
    using callable_type_t = typename callable_traits<T>::type;

    template<typename T>
    using callable_result_t = typename callable_traits<T>::result;

    template<typename T>
    using callable_arguments_t = typename callable_traits<T>::arguments;

    template<std::size_t I, typename... Ts>
    using element_t = std::tuple_element_t<I, std::tuple<Ts...>>;

    template<typename... Ts>
    using first_element_t = element_t<0, Ts...>;

    template<typename T, typename... Ts>
    inline constexpr bool all_same_as_v = (std::is_same_v<T, Ts> && ...);

    template<typename... Ts>
    inline constexpr bool all_same_v = all_same_as_v<first_element_t<Ts...>, Ts...>;
}

#endif //ZERO_TYPE_TRAITS_H
