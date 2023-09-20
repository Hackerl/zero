#ifndef ZERO_TYPE_TRAITS_H
#define ZERO_TYPE_TRAITS_H

#include <list>
#include <array>
#include <tuple>
#include <vector>
#include <functional>
#include <type_traits>

namespace zero::detail {
    template<typename, template<typename ...> class>
    inline constexpr bool is_specialization = false;

    template<template<typename ...> class T, typename ...Args>
    inline constexpr bool is_specialization<T<Args...>, T> = true;

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
    struct callable_traits : public callable_traits<decltype(&T::operator())> {

    };

    template<typename T, typename ReturnType, typename... Args>
    struct callable_traits<ReturnType(T::*)(Args...)> {
        typedef ReturnType result;
        typedef std::tuple<Args...> arguments;
        typedef std::function<ReturnType(Args...)> type;
    };

    template<typename T, typename ReturnType, typename... Args>
    struct callable_traits<ReturnType(T::*)(Args...) const> {
        typedef ReturnType result;
        typedef std::tuple<Args...> arguments;
        typedef std::function<ReturnType(Args...)> type;
    };

    template<typename T>
    using callable_type_t = typename callable_traits<T>::type;

    template<typename T>
    using callable_result_t = typename callable_traits<T>::result;

    template<typename T>
    using callable_arguments_t = typename callable_traits<T>::arguments;

    template<typename...Ts>
    using first_element_t = std::tuple_element_t<0, std::tuple<Ts...>>;

    template<typename T, typename...Ts>
    inline constexpr bool is_elements_same_v = (std::is_same_v<T, Ts> && ...);
}

#endif //ZERO_TYPE_TRAITS_H
