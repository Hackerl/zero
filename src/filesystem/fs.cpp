#include <zero/filesystem/fs.h>
#include <fstream>

#ifdef _WIN32
#include <array>
#include <windows.h>
#elif defined(__APPLE__)
#include <array>
#include <mach-o/dyld.h>
#include <sys/param.h>
#include <zero/expect.h>
#include <zero/os/unix/error.h>
#include <zero/filesystem/fs.h>
#elif defined(__linux__)
#include <zero/filesystem/fs.h>
#endif

std::expected<std::filesystem::path, std::error_code> zero::filesystem::applicationPath() {
#ifdef _WIN32
    std::array<WCHAR, MAX_PATH> buffer{};

    if (const auto length = GetModuleFileNameW(nullptr, buffer.data(), buffer.size());
        length == 0 || length == buffer.size())
        return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

    return buffer.data();
#elif defined(__linux__)
    return readSymlink("/proc/self/exe");
#elif defined(__APPLE__)
    std::array<char, MAXPATHLEN> buffer{};
    auto size = static_cast<std::uint32_t>(buffer.size());

    EXPECT(os::unix::expected([&] {
        return _NSGetExecutablePath(buffer.data(), &size);
    }));

    return weaklyCanonical(buffer.data());
#else
#error "unsupported platform"
#endif
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::applicationDirectory() {
    return applicationPath().transform(&std::filesystem::path::parent_path);
}

std::expected<std::vector<std::byte>, std::error_code> zero::filesystem::read(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};

    if (!stream.is_open())
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    std::vector<std::byte> content;

    while (true) {
        std::array<std::byte, 1024> buffer; // NOLINT(*-pro-type-member-init)

        stream.read(reinterpret_cast<char *>(buffer.data()), buffer.size());

        // waiting for libstdc++ to implement std::vector::append_range
        // content.append_range(std::span{buffer.data(), static_cast<std::size_t>(stream.gcount())});
        content.insert(content.end(), buffer.begin(), buffer.begin() + stream.gcount());

        if (stream.fail()) {
            if (!stream.eof())
                return std::unexpected{std::error_code{errno, std::generic_category()}};

            break;
        }
    }

    return content;
}

std::expected<std::string, std::error_code> zero::filesystem::readString(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};

    if (!stream.is_open())
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    std::string content;

    while (true) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        stream.read(buffer.data(), buffer.size());
        content.append(buffer.data(), stream.gcount());

        if (stream.fail()) {
            if (!stream.eof())
                return std::unexpected{std::error_code{errno, std::generic_category()}};

            break;
        }
    }

    return content;
}

std::expected<void, std::error_code>
zero::filesystem::write(const std::filesystem::path &path, const std::span<const std::byte> content) {
    std::ofstream stream{path, std::ios::binary};

    if (!stream.is_open())
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    if (!stream.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size())))
        return std::unexpected{std::error_code{errno, std::generic_category()}};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::write(const std::filesystem::path &path, const std::string_view content) {
    return write(path, std::as_bytes(std::span{content}));
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::absolute(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::absolute(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::canonical(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::canonical(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code>
zero::filesystem::weaklyCanonical(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = weakly_canonical(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::relative(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::relative(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code>
zero::filesystem::relative(const std::filesystem::path &path, const std::filesystem::path &base) {
    std::error_code ec;
    auto result = std::filesystem::relative(path, base, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::proximate(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::proximate(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code>
zero::filesystem::proximate(const std::filesystem::path &path, const std::filesystem::path &base) {
    std::error_code ec;
    auto result = std::filesystem::proximate(path, base, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<void, std::error_code>
zero::filesystem::copy(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    std::filesystem::copy(from, to, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::copy(
    const std::filesystem::path &from,
    const std::filesystem::path &to,
    const std::filesystem::copy_options options
) {
    std::error_code ec;
    std::filesystem::copy(from, to, options, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::copyFile(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;

    if (const auto result = copy_file(from, to, ec); !result)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::copyFile(
    const std::filesystem::path &from,
    const std::filesystem::path &to,
    const std::filesystem::copy_options options
) {
    std::error_code ec;

    if (const auto result = copy_file(from, to, options, ec); !result)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::copySymlink(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    copy_symlink(from, to, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code> zero::filesystem::createDirectory(const std::filesystem::path &path) {
    std::error_code ec;

    if (const auto result = create_directory(path, ec); !result) {
        if (ec)
            return std::unexpected{ec};

#ifdef _WIN32
        return std::unexpected{std::error_code{ERROR_FILE_EXISTS, std::system_category()}};
#else
        return std::unexpected{std::error_code{EEXIST, std::system_category()}};
#endif
    }

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::createDirectory(const std::filesystem::path &path, const std::filesystem::path &existing) {
    std::error_code ec;

    if (const auto result = create_directory(path, existing, ec); !result)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code> zero::filesystem::createDirectories(const std::filesystem::path &path) {
    std::error_code ec;

    if (const auto result = create_directories(path, ec); !result && ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::createHardLink(const std::filesystem::path &target, const std::filesystem::path &link) {
    std::error_code ec;
    create_hard_link(target, link, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::createSymlink(const std::filesystem::path &target, const std::filesystem::path &link) {
    std::error_code ec;
    create_symlink(target, link, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::createDirectorySymlink(const std::filesystem::path &target, const std::filesystem::path &link) {
    std::error_code ec;
    create_directory_symlink(target, link, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::currentPath() {
    std::error_code ec;
    auto result = std::filesystem::current_path(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<void, std::error_code> zero::filesystem::currentPath(const std::filesystem::path &path) {
    std::error_code ec;
    current_path(path, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<bool, std::error_code> zero::filesystem::exists(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::exists(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code>
zero::filesystem::equivalent(const std::filesystem::path &p1, const std::filesystem::path &p2) {
    std::error_code ec;
    const auto result = std::filesystem::equivalent(p1, p2, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::uintmax_t, std::error_code> zero::filesystem::fileSize(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = file_size(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::uintmax_t, std::error_code> zero::filesystem::hardLinkCount(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = hard_link_count(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::file_time_type, std::error_code>
zero::filesystem::lastWriteTime(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = last_write_time(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<void, std::error_code>
zero::filesystem::lastWriteTime(const std::filesystem::path &path, const std::filesystem::file_time_type time) {
    std::error_code ec;
    last_write_time(path, time, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::permissions(
    const std::filesystem::path &path,
    const std::filesystem::perms perms,
    const std::filesystem::perm_options opts
) {
    std::error_code ec;
    std::filesystem::permissions(path, perms, opts, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::readSymlink(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = read_symlink(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<void, std::error_code> zero::filesystem::remove(const std::filesystem::path &path) {
    std::error_code ec;

    if (const auto result = std::filesystem::remove(path, ec); !result) {
        if (ec)
            return std::unexpected{ec};

#ifdef _WIN32
        return std::unexpected{std::error_code{ERROR_FILE_NOT_FOUND, std::system_category()}};
#else
        return std::unexpected{std::error_code{ENOENT, std::system_category()}};
#endif
    }

    return {};
}

std::expected<std::uintmax_t, std::error_code> zero::filesystem::removeAll(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = remove_all(path, ec);

    if (result == -1)
        return std::unexpected{ec};

    if (result == 0) {
#ifdef _WIN32
        return std::unexpected{std::error_code{ERROR_FILE_NOT_FOUND, std::system_category()}};
#else
        return std::unexpected{std::error_code{ENOENT, std::system_category()}};
#endif
    }

    return result;
}

std::expected<void, std::error_code>
zero::filesystem::rename(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::resizeFile(const std::filesystem::path &path, const std::uintmax_t size) {
    std::error_code ec;
    resize_file(path, size, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<std::filesystem::space_info, std::error_code> zero::filesystem::space(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::space(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::file_status, std::error_code>
zero::filesystem::status(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::status(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::file_status, std::error_code>
zero::filesystem::symlinkStatus(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = symlink_status(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::path, std::error_code> zero::filesystem::temporaryDirectory() {
    std::error_code ec;
    auto result = std::filesystem::temp_directory_path(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isBlockFile(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_block_file(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isCharacterFile(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_character_file(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_directory(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isEmpty(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_empty(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isFIFO(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_fifo(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isOther(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_other(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isRegularFile(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_regular_file(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isSocket(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_socket(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::isSymlink(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_symlink(path, ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

zero::filesystem::DirectoryEntry::DirectoryEntry(std::filesystem::directory_entry entry) : mEntry{std::move(entry)} {
}

std::expected<void, std::error_code> zero::filesystem::DirectoryEntry::assign(const std::filesystem::path &path) {
    std::error_code ec;
    mEntry.assign(path, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code>
zero::filesystem::DirectoryEntry::replaceFilename(const std::filesystem::path &path) {
    std::error_code ec;
    mEntry.replace_filename(path, ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

std::expected<void, std::error_code> zero::filesystem::DirectoryEntry::refresh() {
    std::error_code ec;
    mEntry.refresh(ec);

    if (ec)
        return std::unexpected{ec};

    return {};
}

const std::filesystem::path &zero::filesystem::DirectoryEntry::path() const {
    return mEntry.path();
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::exists() const {
    std::error_code ec;
    const auto result = mEntry.exists(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isBlockFile() const {
    std::error_code ec;
    const auto result = mEntry.is_block_file(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isCharacterFile() const {
    std::error_code ec;
    const auto result = mEntry.is_character_file(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isDirectory() const {
    std::error_code ec;
    const auto result = mEntry.is_directory(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isFIFO() const {
    std::error_code ec;
    const auto result = mEntry.is_fifo(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isOther() const {
    std::error_code ec;
    const auto result = mEntry.is_other(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isRegularFile() const {
    std::error_code ec;
    const auto result = mEntry.is_regular_file(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isSocket() const {
    std::error_code ec;
    const auto result = mEntry.is_socket(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isSymlink() const {
    std::error_code ec;
    const auto result = mEntry.is_symlink(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::uintmax_t, std::error_code> zero::filesystem::DirectoryEntry::fileSize() const {
    std::error_code ec;
    const auto result = mEntry.file_size(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::uintmax_t, std::error_code> zero::filesystem::DirectoryEntry::hardLinkCount() const {
    std::error_code ec;
    const auto result = mEntry.hard_link_count(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::file_time_type, std::error_code>
zero::filesystem::DirectoryEntry::lastWriteTime() const {
    std::error_code ec;
    const auto result = mEntry.last_write_time(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::file_status, std::error_code> zero::filesystem::DirectoryEntry::status() const {
    std::error_code ec;
    const auto result = mEntry.status(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

std::expected<std::filesystem::file_status, std::error_code> zero::filesystem::DirectoryEntry::symlinkStatus() const {
    std::error_code ec;
    const auto result = mEntry.symlink_status(ec);

    if (ec)
        return std::unexpected{ec};

    return result;
}

zero::filesystem::EntryProxy::EntryProxy(std::expected<DirectoryEntry, std::error_code> entry)
    : mEntry{std::move(entry)} {
}

const std::expected<zero::filesystem::DirectoryEntry, std::error_code> &
zero::filesystem::EntryProxy::operator*() const & {
    return mEntry;
}

std::expected<zero::filesystem::DirectoryEntry, std::error_code> zero::filesystem::EntryProxy::operator*() && {
    return std::move(mEntry);
}

std::expected<zero::filesystem::NoExcept<std::filesystem::directory_iterator>, std::error_code>
zero::filesystem::readDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::directory_iterator it{path, ec};

    if (ec)
        return std::unexpected{ec};

    return NoExcept{std::move(it)};
}

std::expected<zero::filesystem::NoExcept<std::filesystem::recursive_directory_iterator>, std::error_code>
zero::filesystem::walkDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it{path, ec};

    if (ec)
        return std::unexpected{ec};

    return NoExcept{std::move(it)};
}
