#ifndef ZERO_DEFER_H
#define ZERO_DEFER_H

#include <utility>

namespace zero {
    template<typename F>
    class Defer {
    public:
        explicit Defer(F &&fn) : mFn{std::move(fn)} {
        }

        ~Defer() {
            mFn();
        }

    private:
        F mFn;
    };
}

#define Z_DEFER_NAME(x, y) x##y
#define Z_DEFER_VARIABLE(x, y) Z_DEFER_NAME(x, y)
#define Z_DEFER(code) zero::Defer Z_DEFER_VARIABLE(defer, __COUNTER__){[&](){ code; }}

#endif //ZERO_DEFER_H
