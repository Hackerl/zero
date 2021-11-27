#ifndef ZERO_EVENT_H
#define ZERO_EVENT_H

#include <ctime>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace zero {
    namespace atomic {
        class CEvent {
#ifdef _WIN32
        public:
            CEvent();
            ~CEvent();
#endif
        public:
            void wait(const int *timeout = nullptr);

        public:
            void notify();
            void broadcast();

        private:
#ifdef _WIN32
            long mCore{};
            HANDLE mMEvent;
            HANDLE mAEvent;
#elif __linux__
            int mCore{};
#endif
        };
    }
}

#endif //ZERO_EVENT_H
