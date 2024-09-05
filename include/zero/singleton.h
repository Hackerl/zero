#ifndef ZERO_SINGLETON_H
#define ZERO_SINGLETON_H

namespace zero {
    template<typename T>
    struct Singleton {
        static T &instance() {
            static T instance = {};
            return instance;
        }
    };
}

#endif //ZERO_SINGLETON_H
