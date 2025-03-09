#ifndef ZERO_RESOURCE_H
#define ZERO_RESOURCE_H

#include <expected>
#include <system_error>
#include <zero/io/io.h>

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
        [[nodiscard]] std::expected<bool, std::error_code> isInheritable() const;
        [[nodiscard]] std::expected<Resource, std::error_code> duplicate(bool inheritable = false) const;

        std::expected<void, std::error_code> setInheritable(bool inheritable);

        [[nodiscard]] Native release();
        std::expected<void, std::error_code> close();

    private:
        Native mNative;
    };

#ifdef _WIN32
    class IOResource final
#else
    class IOResource
#endif
        : public io::IFileDescriptor, public io::IReader, public io::IWriter, public io::ICloseable,
          public io::ISeekable {
    public:
        explicit IOResource(Resource::Native native);
        explicit IOResource(Resource resource);

        [[nodiscard]] io::FileDescriptor fd() const override;
        [[nodiscard]] bool valid() const;
        [[nodiscard]] explicit operator bool() const;
        [[nodiscard]] std::expected<bool, std::error_code> isInheritable() const;
        [[nodiscard]] std::expected<IOResource, std::error_code> duplicate(bool inheritable = false) const;

        std::expected<std::size_t, std::error_code> read(std::span<std::byte> data) override;
        std::expected<std::size_t, std::error_code> write(std::span<const std::byte> data) override;
        std::expected<std::uint64_t, std::error_code> seek(std::int64_t offset, Whence whence) override;
        std::expected<void, std::error_code> close() override;

        std::expected<void, std::error_code> setInheritable(bool inheritable);
        [[nodiscard]] io::FileDescriptor release();

    private:
        Resource mResource;
    };
}

#endif //ZERO_RESOURCE_H
