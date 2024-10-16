#include <zero/detail/type_traits.h>
#include <vector>
#include <list>

void func1(short, int, long) {
}

void func2(int, int, int) {
}

void func3(short, int) {
}

static_assert(zero::detail::is_specialization_v<std::vector<int>, std::vector>);
static_assert(zero::detail::is_specialization_v<std::tuple<short, int, long>, std::tuple>);
static_assert(!zero::detail::is_specialization_v<std::vector<int>, std::list>);

static_assert(zero::detail::is_applicable_v<decltype(&func1), std::tuple<short, int, long>>);
static_assert(zero::detail::is_applicable_v<decltype(&func2), std::array<int, 3>>);
static_assert(zero::detail::is_applicable_v<decltype(&func3), std::pair<short, int>>);

static_assert(std::is_same_v<zero::detail::element_t<1, short, int, long>, int>);
static_assert(std::is_same_v<zero::detail::first_element_t<short, int, long>, short>);
static_assert(zero::detail::all_same_v<short, short, short>);

static_assert(std::is_void_v<zero::detail::function_result_t<decltype(&func1)>>);

static_assert(
    std::is_same_v<
        zero::detail::function_arguments_t<decltype(&func1)>,
        std::tuple<short, int, long>
    >
);
