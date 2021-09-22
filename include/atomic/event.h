#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <ctime>

namespace zero {
    namespace atomic {
        class CEvent {
        public:
            void wait(const timespec *timeout = nullptr);

        public:
            void notify();
            void broadcast();

        private:
            int mCore{};
        };
    }
}

#endif //ZERO_EVENT_H
