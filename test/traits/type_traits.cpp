#include <zero/traits/type_traits.h>
#include <zero/interface.h>
#include <vector>
#include <list>

namespace {
    void func1(short, int, long) {
    }

    void func2(int, int, int) {
    }

    void func3(short, int) {
    }

    class Interface : public zero::Interface {
        virtual void test() = 0;
    };

    class Implement final : public Interface {
        void test() override {
        }
    };
}

static_assert(zero::traits::is_specialization_v<std::vector<int>, std::vector>);
static_assert(zero::traits::is_specialization_v<std::tuple<short, int, long>, std::tuple>);
static_assert(!zero::traits::is_specialization_v<std::vector<int>, std::list>);

static_assert(zero::traits::is_applicable_v<decltype(&func1), std::tuple<short, int, long>>);
static_assert(zero::traits::is_applicable_v<decltype(&func2), std::array<int, 3>>);
static_assert(zero::traits::is_applicable_v<decltype(&func3), std::pair<short, int>>);

static_assert(std::is_same_v<zero::traits::element_t<1, short, int, long>, int>);
static_assert(std::is_same_v<zero::traits::first_element_t<short, int, long>, short>);
static_assert(zero::traits::all_same_v<short, short, short>);

static_assert(std::is_void_v<zero::traits::function_result_t<decltype(&func1)>>);

static_assert(
    std::is_same_v<
        zero::traits::function_arguments_t<decltype(&func1)>,
        std::tuple<short, int, long>
    >
);

static_assert(zero::traits::Trait<Implement, Interface>);
static_assert(zero::traits::Trait<std::shared_ptr<Interface>, Interface>);
static_assert(zero::traits::Trait<std::unique_ptr<Interface>, Interface>);
static_assert(zero::traits::Trait<std::shared_ptr<Implement>, Interface>);
static_assert(zero::traits::Trait<std::unique_ptr<Implement>, Interface>);

static_assert(zero::traits::Trait<const Implement, const Interface>);
static_assert(zero::traits::Trait<std::shared_ptr<const Interface>, const Interface>);
static_assert(zero::traits::Trait<std::unique_ptr<const Interface>, const Interface>);
static_assert(zero::traits::Trait<std::shared_ptr<const Implement>, const Interface>);
static_assert(zero::traits::Trait<std::unique_ptr<const Implement>, const Interface>);

static_assert(!zero::traits::Trait<const Implement, Interface>);
static_assert(!zero::traits::Trait<std::shared_ptr<const Interface>, Interface>);
static_assert(!zero::traits::Trait<std::unique_ptr<const Interface>, Interface>);
static_assert(!zero::traits::Trait<std::shared_ptr<const Implement>, Interface>);
static_assert(!zero::traits::Trait<std::unique_ptr<const Implement>, Interface>);
