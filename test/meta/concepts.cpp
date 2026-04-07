#include <zero/meta/concepts.h>
#include <vector>
#include <list>

namespace {
    struct Interface {
        virtual ~Interface() = default;
        virtual void test() = 0;
    };

    struct Implement final : Interface {
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

static_assert(zero::meta::Mutable<int>);
static_assert(zero::meta::Mutable<int &>);
static_assert(zero::meta::Mutable<int &&>);
static_assert(!zero::meta::Mutable<const int>);
static_assert(!zero::meta::Mutable<const int &>);
static_assert(!zero::meta::Mutable<const int &&>);
