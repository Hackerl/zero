#ifndef ZERO_DEFER_H
#define ZERO_DEFER_H

#include <utility>

namespace zero {
    template<typename F>
    class Defer {
    public:
        explicit Defer(F &&fn) : mFn(std::move(fn)) {

        }

        ~Defer() {
            mFn();
        }

    private:
        F mFn;
    };
}

#define DEFER_NAME(x, y) x##y
#define DEFER_VARIABLE(x, y) DEFER_NAME(x, y)
#define DEFER(code) zero::Defer DEFER_VARIABLE(defer, __COUNTER__)([&](){ code; })

#endif //ZERO_DEFER_H
