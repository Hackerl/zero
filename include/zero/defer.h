#ifndef ZERO_DEFER_H
#define ZERO_DEFER_H

#include <utility>

namespace zero {
    template<std::invocable F>
    class Defer {
    public:
        explicit Defer(F &&f) : mFunction{std::move(f)} {
        }

        ~Defer() {
            mFunction();
        }

    private:
        F mFunction;
    };
}

#define Z_DEFER_NAME(x, y) x##y
#define Z_DEFER_VARIABLE(x, y) Z_DEFER_NAME(x, y)
#define Z_DEFER(...) zero::Defer Z_DEFER_VARIABLE(defer, __COUNTER__){[&](){ __VA_ARGS__; }}

#endif //ZERO_DEFER_H
