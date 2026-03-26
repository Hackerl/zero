#include <zero/meta/concepts.h>
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

static_assert(zero::meta::Trait<Implement, Interface>);
static_assert(zero::meta::Trait<std::shared_ptr<Interface>, Interface>);
static_assert(zero::meta::Trait<std::unique_ptr<Interface>, Interface>);
static_assert(zero::meta::Trait<std::shared_ptr<Implement>, Interface>);
static_assert(zero::meta::Trait<std::unique_ptr<Implement>, Interface>);

static_assert(zero::meta::Trait<const Implement, const Interface>);
static_assert(zero::meta::Trait<std::shared_ptr<const Interface>, const Interface>);
static_assert(zero::meta::Trait<std::unique_ptr<const Interface>, const Interface>);
static_assert(zero::meta::Trait<std::shared_ptr<const Implement>, const Interface>);
static_assert(zero::meta::Trait<std::unique_ptr<const Implement>, const Interface>);

static_assert(!zero::meta::Trait<const Implement, Interface>);
static_assert(!zero::meta::Trait<std::shared_ptr<const Interface>, Interface>);
static_assert(!zero::meta::Trait<std::unique_ptr<const Interface>, Interface>);
static_assert(!zero::meta::Trait<std::shared_ptr<const Implement>, Interface>);
static_assert(!zero::meta::Trait<std::unique_ptr<const Implement>, Interface>);

static_assert(zero::meta::Specialization<std::vector<int>, std::vector>);
static_assert(zero::meta::Specialization<std::tuple<short, int, long>, std::tuple>);
static_assert(!zero::meta::Specialization<std::vector<int>, std::list>);

static_assert(zero::meta::Applicable<decltype(&func1), std::tuple<short, int, long>>);
static_assert(zero::meta::Applicable<decltype(&func2), std::array<int, 3>>);
static_assert(zero::meta::Applicable<decltype(&func3), std::pair<short, int>>);
