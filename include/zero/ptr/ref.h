#ifndef ZERO_REF_H
#define ZERO_REF_H

#include <atomic>

namespace zero::ptr {
    class RefCounter {
    public:
        RefCounter() : mCount(0) {

        }

        RefCounter(const RefCounter &) : mCount(0) {

        }

        virtual ~RefCounter() = default;

    public:
        RefCounter &operator=(const RefCounter &) {
            return *this;
        }

    protected:
        void addRef() {
            mCount++;
        }

        void release() {
            if (--mCount == 0) {
                delete this;
            }
        }

        [[nodiscard]] long useCount() const {
            return mCount;
        }

    private:
        std::atomic<long> mCount;

        template<typename T>
        friend class RefPtr;
    };

    template<typename T>
    class RefPtr {
    public:
        RefPtr() : mPtr(nullptr) {

        }

        RefPtr(T *ptr) : mPtr(ptr) {
            if (mPtr)
                mPtr->addRef();
        }

        RefPtr(const RefPtr &rhs) : mPtr(rhs.mPtr) {
            if (mPtr)
                mPtr->addRef();
        }

        template<typename U>
        RefPtr(const RefPtr<U> &rhs) : mPtr(rhs.get()) {
            if (mPtr)
                mPtr->addRef();
        }

        RefPtr(RefPtr &&rhs) noexcept: mPtr(nullptr) {
            std::swap(mPtr, rhs.mPtr);
        }

        template<typename U>
        RefPtr(RefPtr<U> &&rhs) : mPtr(rhs.mPtr) {
            rhs.mPtr = nullptr;
        }

        ~RefPtr() {
            if (mPtr)
                mPtr->release();
        }

    public:
        RefPtr &operator=(const RefPtr &rhs) {
            RefPtr(rhs).swap(*this);
            return *this;
        }

        template<typename U>
        RefPtr &operator=(const RefPtr<U> &rhs) {
            RefPtr(rhs).swap(*this);
            return *this;
        }

        RefPtr &operator=(T *rhs) {
            RefPtr(rhs).swap(*this);
            return *this;
        }

        RefPtr &operator=(RefPtr &&rhs) noexcept {
            RefPtr(std::move(rhs)).swap(*this);
            return *this;
        }

        template<typename U>
        RefPtr &operator=(RefPtr<U> &&rhs) {
            RefPtr(std::move(rhs)).swap(*this);
            return *this;
        }

    public:
        [[nodiscard]] T *get() const {
            return mPtr;
        }

        [[nodiscard]] long useCount() const {
            if (!mPtr)
                return 0;

            return mPtr->useCount();
        }

        void reset() {
            RefPtr().swap(*this);
        }

        void reset(T *rhs) {
            RefPtr(rhs).swap(*this);
        }

        void swap(RefPtr &rhs) {
            std::swap(mPtr, rhs.mPtr);
        }

    public:
        explicit operator bool() const {
            return mPtr;
        }

        [[nodiscard]] T *operator->() const {
            return mPtr;
        }

        [[nodiscard]] T &operator*() const {
            return *mPtr;
        }

    private:
        T *mPtr;

        template<typename U>
        friend class RefPtr;
    };

    template<typename T, typename ...Args>
    RefPtr<T> makeRef(Args &&... args) {
        return RefPtr<T>(new T(std::forward<Args>(args)...));
    }
}

#endif //ZERO_REF_H
