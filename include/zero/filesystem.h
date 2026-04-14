#ifndef ZERO_FILESYSTEM_H
#define ZERO_FILESYSTEM_H

#include <span>
#include <vector>
#include <optional>
#include <expected>
#include <filesystem>

namespace zero::filesystem {
    /*
     * Encoding issues on Windows are always a headache.
     * `std::filesystem::path` uses `ANSI` encoding by default for `std::string`,
     * which often causes unexpected encoding exceptions when building cross-platform applications.
     * I want to standardize the string encoding to `UTF-8`, so I created these two functions.
     */
    std::filesystem::path path(std::string_view source);
    std::string stringify(const std::filesystem::path &path);

    std::filesystem::path applicationPath();
    std::filesystem::path applicationDirectory();

    std::expected<std::vector<std::byte>, std::error_code> read(const std::filesystem::path &path);
    std::expected<std::string, std::error_code> readString(const std::filesystem::path &path);

    std::expected<void, std::error_code> write(const std::filesystem::path &path, std::span<const std::byte> content);
    std::expected<void, std::error_code> write(const std::filesystem::path &path, std::string_view content);

    std::filesystem::path absolute(const std::filesystem::path &path);
    std::expected<std::filesystem::path, std::error_code> canonical(const std::filesystem::path &path);
    std::expected<std::filesystem::path, std::error_code> weaklyCanonical(const std::filesystem::path &path);
    std::expected<std::filesystem::path, std::error_code> relative(const std::filesystem::path &path);

    std::expected<std::filesystem::path, std::error_code>
    relative(const std::filesystem::path &path, const std::filesystem::path &base);

    std::expected<std::filesystem::path, std::error_code> proximate(const std::filesystem::path &path);

    std::expected<std::filesystem::path, std::error_code>
    proximate(const std::filesystem::path &path, const std::filesystem::path &base);

    std::expected<void, std::error_code> copy(const std::filesystem::path &from, const std::filesystem::path &to);

    std::expected<void, std::error_code>
    copy(const std::filesystem::path &from, const std::filesystem::path &to, std::filesystem::copy_options options);

    std::expected<void, std::error_code> copyFile(const std::filesystem::path &from, const std::filesystem::path &to);

    std::expected<void, std::error_code>
    copyFile(const std::filesystem::path &from, const std::filesystem::path &to, std::filesystem::copy_options options);

    std::expected<void, std::error_code>
    copySymlink(const std::filesystem::path &from, const std::filesystem::path &to);

    std::expected<void, std::error_code> createDirectory(const std::filesystem::path &path);

    std::expected<void, std::error_code>
    createDirectory(const std::filesystem::path &path, const std::filesystem::path &existing);

    std::expected<void, std::error_code> createDirectories(const std::filesystem::path &path);

    std::expected<void, std::error_code>
    createHardLink(const std::filesystem::path &target, const std::filesystem::path &link);

    std::expected<void, std::error_code>
    createSymlink(const std::filesystem::path &target, const std::filesystem::path &link);

    std::expected<void, std::error_code>
    createDirectorySymlink(const std::filesystem::path &target, const std::filesystem::path &link);

    std::filesystem::path currentPath();
    std::expected<void, std::error_code> currentPath(const std::filesystem::path &path);

    std::expected<bool, std::error_code> exists(const std::filesystem::path &path);

    std::expected<bool, std::error_code> equivalent(const std::filesystem::path &p1, const std::filesystem::path &p2);

    std::expected<std::uintmax_t, std::error_code> fileSize(const std::filesystem::path &path);
    std::expected<std::uintmax_t, std::error_code> hardLinkCount(const std::filesystem::path &path);

    std::expected<std::filesystem::file_time_type, std::error_code> lastWriteTime(const std::filesystem::path &path);

    std::expected<void, std::error_code>
    lastWriteTime(const std::filesystem::path &path, std::filesystem::file_time_type time);

    std::expected<void, std::error_code>
    permissions(
        const std::filesystem::path &path,
        std::filesystem::perms perms,
        std::filesystem::perm_options opts = std::filesystem::perm_options::replace
    );

    std::expected<std::filesystem::path, std::error_code> readSymlink(const std::filesystem::path &path);

    std::expected<void, std::error_code> remove(const std::filesystem::path &path);
    std::expected<std::uintmax_t, std::error_code> removeAll(const std::filesystem::path &path);

    std::expected<void, std::error_code> rename(const std::filesystem::path &from, const std::filesystem::path &to);
    std::expected<void, std::error_code> resizeFile(const std::filesystem::path &path, std::uintmax_t size);
    std::expected<std::filesystem::space_info, std::error_code> space(const std::filesystem::path &path);
    std::expected<std::filesystem::file_status, std::error_code> status(const std::filesystem::path &path);
    std::expected<std::filesystem::file_status, std::error_code> symlinkStatus(const std::filesystem::path &path);
    std::filesystem::path temporaryDirectory();

    std::expected<bool, std::error_code> isBlockFile(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isCharacterFile(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isDirectory(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isEmpty(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isFIFO(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isOther(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isRegularFile(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isSocket(const std::filesystem::path &path);
    std::expected<bool, std::error_code> isSymlink(const std::filesystem::path &path);

    class DirectoryEntry {
    public:
        explicit DirectoryEntry(std::filesystem::directory_entry entry);

        std::expected<void, std::error_code> assign(const std::filesystem::path &path);
        std::expected<void, std::error_code> replaceFilename(const std::filesystem::path &path);
        std::expected<void, std::error_code> refresh();

        [[nodiscard]] const std::filesystem::path &path() const;
        [[nodiscard]] std::expected<bool, std::error_code> exists() const;

        [[nodiscard]] std::expected<bool, std::error_code> isBlockFile() const;
        [[nodiscard]] std::expected<bool, std::error_code> isCharacterFile() const;
        [[nodiscard]] std::expected<bool, std::error_code> isDirectory() const;
        [[nodiscard]] std::expected<bool, std::error_code> isFIFO() const;
        [[nodiscard]] std::expected<bool, std::error_code> isOther() const;
        [[nodiscard]] std::expected<bool, std::error_code> isRegularFile() const;
        [[nodiscard]] std::expected<bool, std::error_code> isSocket() const;
        [[nodiscard]] std::expected<bool, std::error_code> isSymlink() const;

        [[nodiscard]] std::expected<std::uintmax_t, std::error_code> fileSize() const;
        [[nodiscard]] std::expected<std::uintmax_t, std::error_code> hardLinkCount() const;
        [[nodiscard]] std::expected<std::filesystem::file_time_type, std::error_code> lastWriteTime() const;
        [[nodiscard]] std::expected<std::filesystem::file_status, std::error_code> status() const;
        [[nodiscard]] std::expected<std::filesystem::file_status, std::error_code> symlinkStatus() const;

    private:
        std::filesystem::directory_entry mEntry;
    };

    template<typename T>
    class NoExcept {
        static_assert(
            std::is_same_v<T, std::filesystem::directory_iterator> ||
            std::is_same_v<T, std::filesystem::recursive_directory_iterator>
        );

    public:
        explicit NoExcept(T it) : mIterator{std::move(it)}, mStarted{false} {
        }

        std::expected<std::optional<DirectoryEntry>, std::error_code> next() {
            if (mIterator == std::default_sentinel)
                return std::nullopt;

            if (!mStarted) {
                mStarted = true;
                return DirectoryEntry{*mIterator};
            }

            std::error_code ec;
            mIterator.increment(ec);

            if (ec)
                return std::unexpected{ec};

            if (mIterator == std::default_sentinel)
                return std::nullopt;

            return DirectoryEntry{*mIterator};
        }

    private:
        T mIterator;
        bool mStarted;
    };

    std::expected<NoExcept<std::filesystem::directory_iterator>, std::error_code>
    readDirectory(const std::filesystem::path &path);

    std::expected<NoExcept<std::filesystem::directory_iterator>, std::error_code>
    readDirectory(const std::filesystem::path &path, std::filesystem::directory_options options);

    std::expected<NoExcept<std::filesystem::recursive_directory_iterator>, std::error_code>
    walkDirectory(const std::filesystem::path &path);

    std::expected<NoExcept<std::filesystem::recursive_directory_iterator>, std::error_code>
    walkDirectory(const std::filesystem::path &path, std::filesystem::directory_options options);
}

#endif //ZERO_FILESYSTEM_H
