#ifndef ZERO_META_CONCEPTS_H
#define ZERO_META_CONCEPTS_H

#include <memory>
#include <concepts>
#include "type_traits.h"

namespace zero::meta {
    template<typename T, typename I>
    concept Trait = std::convertible_to<std::remove_cvref_t<T>, std::shared_ptr<I>> || (
        std::derived_from<std::remove_cvref_t<T>, std::remove_const_t<I>> &&
        std::convertible_to<std::add_lvalue_reference_t<T>, I &>
    );

    template<typename T, template<typename...> class Template>
    concept Specialization = IsSpecialization<T, Template>;

    template<typename T>
    concept Pointer = std::is_pointer_v<T>;

    template<typename T>
    concept Mutable = !std::is_const_v<std::remove_reference_t<T>>;
}

#endif //ZERO_META_CONCEPTS_H
