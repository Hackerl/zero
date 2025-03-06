#ifndef ZERO_RESOURCE_H
#define ZERO_RESOURCE_H

#include <expected>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace zero::os {
    class Resource {
    public:
#ifdef _WIN32
        using Native = HANDLE;
#else
        using Native = int;
#endif

        explicit Resource(Native native);
        Resource(Resource &&rhs) noexcept;
        Resource &operator=(Resource &&rhs) noexcept;
        ~Resource();

        [[nodiscard]] Native get() const;
        [[nodiscard]] Native operator*() const;
        [[nodiscard]] bool valid() const;
        [[nodiscard]] explicit operator bool() const;
        [[nodiscard]] std::expected<bool, std::error_code> isInherited() const;
        [[nodiscard]] std::expected<Resource, std::error_code> duplicate(bool inherited = false) const;

        std::expected<void, std::error_code> setInherited(bool inherited);

        [[nodiscard]] Native release();
        std::expected<void, std::error_code> close();

    private:
        Native mNative;
    };
}

#endif //ZERO_RESOURCE_H
