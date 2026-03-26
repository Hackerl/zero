#include <zero/meta/type_traits.h>
#include <vector>
#include <list>

namespace {
    void func1(short, int, long) {
    }

    void func2(int, int, int) {
    }

    void func3(short, int) {
    }
}

static_assert(zero::meta::IsSpecialization<std::vector<int>, std::vector>);
static_assert(zero::meta::IsSpecialization<std::tuple<short, int, long>, std::tuple>);
static_assert(!zero::meta::IsSpecialization<std::vector<int>, std::list>);

static_assert(zero::meta::IsApplicable<decltype(&func1), std::tuple<short, int, long>>);
static_assert(zero::meta::IsApplicable<decltype(&func2), std::array<int, 3>>);
static_assert(zero::meta::IsApplicable<decltype(&func3), std::pair<short, int>>);

static_assert(zero::meta::IsAllSame<short, short, short>);

static_assert(std::is_same_v<zero::meta::Element<1, short, int, long>, int>);
static_assert(std::is_same_v<zero::meta::FirstElement<short, int, long>, short>);

static_assert(std::is_void_v<zero::meta::FunctionResult<decltype(&func1)>>);

static_assert(
    std::is_same_v<
        zero::meta::FunctionArguments<decltype(&func1)>,
        std::tuple<short, int, long>
    >
);
