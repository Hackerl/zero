#ifndef ZERO_FILESYSTEM_STD_H
#define ZERO_FILESYSTEM_STD_H

#include <filesystem>
#include <tl/expected.hpp>

namespace zero::filesystem {
    tl::expected<std::filesystem::path, std::error_code> absolute(const std::filesystem::path &path);
    tl::expected<std::filesystem::path, std::error_code> canonical(const std::filesystem::path &path);
    tl::expected<std::filesystem::path, std::error_code> weaklyCanonical(const std::filesystem::path &path);
    tl::expected<std::filesystem::path, std::error_code> relative(const std::filesystem::path &path);

    tl::expected<std::filesystem::path, std::error_code>
    relative(const std::filesystem::path &path, const std::filesystem::path &base);

    tl::expected<std::filesystem::path, std::error_code> proximate(const std::filesystem::path &path);

    tl::expected<std::filesystem::path, std::error_code>
    proximate(const std::filesystem::path &path, const std::filesystem::path &base);

    tl::expected<void, std::error_code> copy(const std::filesystem::path &from, const std::filesystem::path &to);

    tl::expected<void, std::error_code>
    copy(const std::filesystem::path &from, const std::filesystem::path &to, std::filesystem::copy_options options);

    tl::expected<bool, std::error_code> copyFile(const std::filesystem::path &from, const std::filesystem::path &to);

    tl::expected<bool, std::error_code>
    copyFile(const std::filesystem::path &from, const std::filesystem::path &to, std::filesystem::copy_options options);

    tl::expected<void, std::error_code>
    copySymlink(const std::filesystem::path &from, const std::filesystem::path &to);

    tl::expected<bool, std::error_code> createDirectory(const std::filesystem::path &path);

    tl::expected<bool, std::error_code>
    createDirectory(const std::filesystem::path &path, const std::filesystem::path &existing);

    tl::expected<bool, std::error_code> createDirectories(const std::filesystem::path &path);

    tl::expected<void, std::error_code>
    createHardLink(const std::filesystem::path &target, const std::filesystem::path &link);

    tl::expected<void, std::error_code>
    createSymlink(const std::filesystem::path &target, const std::filesystem::path &link);

    tl::expected<void, std::error_code>
    createDirectorySymlink(const std::filesystem::path &target, const std::filesystem::path &link);

    tl::expected<std::filesystem::path, std::error_code> currentPath();
    tl::expected<void, std::error_code> currentPath(const std::filesystem::path &path);

    tl::expected<bool, std::error_code> exists(const std::filesystem::path &path);

    tl::expected<bool, std::error_code> equivalent(const std::filesystem::path &p1, const std::filesystem::path &p2);

    tl::expected<std::uintmax_t, std::error_code> fileSize(const std::filesystem::path &path);
    tl::expected<std::uintmax_t, std::error_code> hardLinkCount(const std::filesystem::path &path);

    tl::expected<std::filesystem::file_time_type, std::error_code> lastWriteTime(const std::filesystem::path &path);

    tl::expected<void, std::error_code>
    lastWriteTime(const std::filesystem::path &path, std::filesystem::file_time_type time);

    tl::expected<void, std::error_code>
    permissions(
        const std::filesystem::path &path,
        std::filesystem::perms perms,
        std::filesystem::perm_options opts = std::filesystem::perm_options::replace
    );

    tl::expected<std::filesystem::path, std::error_code> readSymlink(const std::filesystem::path &path);

    tl::expected<bool, std::error_code> remove(const std::filesystem::path &path);
    tl::expected<std::uintmax_t, std::error_code> removeAll(const std::filesystem::path &path);

    tl::expected<void, std::error_code> rename(const std::filesystem::path &from, const std::filesystem::path &to);
    tl::expected<void, std::error_code> resizeFile(const std::filesystem::path &path, std::uintmax_t size);
    tl::expected<std::filesystem::space_info, std::error_code> space(const std::filesystem::path &path);
    tl::expected<std::filesystem::file_status, std::error_code> status(const std::filesystem::path &path);
    tl::expected<std::filesystem::file_status, std::error_code> symlinkStatus(const std::filesystem::path &path);
    tl::expected<std::filesystem::path, std::error_code> temporaryDirectory();

    tl::expected<bool, std::error_code> isBlockFile(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isCharacterFile(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isDirectory(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isEmpty(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isFIFO(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isOther(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isRegularFile(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isSocket(const std::filesystem::path &path);
    tl::expected<bool, std::error_code> isSymlink(const std::filesystem::path &path);

    class DirectoryEntry {
    public:
        explicit DirectoryEntry(std::filesystem::directory_entry entry);

        tl::expected<void, std::error_code> assign(const std::filesystem::path &path);
        tl::expected<void, std::error_code> replaceFilename(const std::filesystem::path &path);
        tl::expected<void, std::error_code> refresh();

        [[nodiscard]] const std::filesystem::path &path() const;
        [[nodiscard]] tl::expected<bool, std::error_code> exists() const;

        [[nodiscard]] tl::expected<bool, std::error_code> isBlockFile() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isCharacterFile() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isDirectory() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isFIFO() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isOther() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isRegularFile() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isSocket() const;
        [[nodiscard]] tl::expected<bool, std::error_code> isSymlink() const;

        [[nodiscard]] tl::expected<std::uintmax_t, std::error_code> fileSize() const;
        [[nodiscard]] tl::expected<std::uintmax_t, std::error_code> hardLinkCount() const;
        [[nodiscard]] tl::expected<std::filesystem::file_time_type, std::error_code> lastWriteTime() const;
        [[nodiscard]] tl::expected<std::filesystem::file_status, std::error_code> status() const;
        [[nodiscard]] tl::expected<std::filesystem::file_status, std::error_code> symlinkStatus() const;

    private:
        std::filesystem::directory_entry mEntry;
    };

    class EntryProxy {
    public:
        explicit EntryProxy(tl::expected<DirectoryEntry, std::error_code> entry);

        [[nodiscard]] const tl::expected<DirectoryEntry, std::error_code> &operator*() const &;
        tl::expected<DirectoryEntry, std::error_code> operator*() &&;

    private:
        tl::expected<DirectoryEntry, std::error_code> mEntry;
    };

    template<
        typename T,
        std::enable_if_t<
            std::is_same_v<T, std::filesystem::directory_iterator> ||
            std::is_same_v<T, std::filesystem::recursive_directory_iterator>
        >* = nullptr
    >
    class NoExcept {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = tl::expected<DirectoryEntry, std::error_code>;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = value_type;

        NoExcept() = default;

        explicit NoExcept(T it) : mIterator{std::move(it)} {
        }

        [[nodiscard]] value_type operator*() const {
            if (mErrorCode)
                return tl::unexpected(mErrorCode);

            return DirectoryEntry{*mIterator};
        }

        NoExcept &operator++() {
            if (mErrorCode) {
                mErrorCode.clear();
                mIterator = {};
                return *this;
            }

            mIterator.increment(mErrorCode);
            return *this;
        }

        EntryProxy operator++(int) {
            EntryProxy proxy{**this};

            if (mErrorCode) {
                mErrorCode.clear();
                mIterator = {};
                return proxy;
            }

            mIterator.increment(mErrorCode);
            return proxy;
        }

        [[nodiscard]] bool operator==(const NoExcept &rhs) const {
            // some standard libraries reset iterator immediately when an error occurs
            if (mErrorCode && rhs.mIterator == T{})
                return false;

            return mIterator == rhs.mIterator;
        }

        [[nodiscard]] bool operator!=(const NoExcept &rhs) const {
            return !operator==(rhs);
        }

    private:
        std::error_code mErrorCode;
        T mIterator;
    };

    template<typename T>
    NoExcept<T> begin(NoExcept<T> it) {
        return it;
    }

    template<typename T>
    NoExcept<T> end(NoExcept<T>) {
        return {};
    }

    tl::expected<NoExcept<std::filesystem::directory_iterator>, std::error_code>
    readDirectory(const std::filesystem::path &path);

    tl::expected<NoExcept<std::filesystem::recursive_directory_iterator>, std::error_code>
    walkDirectory(const std::filesystem::path &path);
}

#endif //ZERO_FILESYSTEM_STD_H
