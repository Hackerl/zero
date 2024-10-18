#include <zero/filesystem/std.h>

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::absolute(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::absolute(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::canonical(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::canonical(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code>
zero::filesystem::weaklyCanonical(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = weakly_canonical(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::relative(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::relative(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code>
zero::filesystem::relative(const std::filesystem::path &path, const std::filesystem::path &base) {
    std::error_code ec;
    auto result = std::filesystem::relative(path, base, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::proximate(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::proximate(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code>
zero::filesystem::proximate(const std::filesystem::path &path, const std::filesystem::path &base) {
    std::error_code ec;
    auto result = std::filesystem::proximate(path, base, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<void, std::error_code>
zero::filesystem::copy(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    std::filesystem::copy(from, to, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::copy(
    const std::filesystem::path &from,
    const std::filesystem::path &to,
    const std::filesystem::copy_options options
) {
    std::error_code ec;
    std::filesystem::copy(from, to, options, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<bool, std::error_code>
zero::filesystem::copyFile(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    const auto result = copy_file(from, to, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code>
zero::filesystem::copyFile(
    const std::filesystem::path &from,
    const std::filesystem::path &to,
    const std::filesystem::copy_options options
) {
    std::error_code ec;
    const auto result = copy_file(from, to, options, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<void, std::error_code>
zero::filesystem::copySymlink(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    copy_symlink(from, to, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<bool, std::error_code> zero::filesystem::createDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = create_directory(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code>
zero::filesystem::createDirectory(const std::filesystem::path &path, const std::filesystem::path &existing) {
    std::error_code ec;
    const auto result = create_directory(path, existing, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::createDirectories(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = create_directories(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<void, std::error_code>
zero::filesystem::createHardLink(const std::filesystem::path &target, const std::filesystem::path &link) {
    std::error_code ec;
    create_hard_link(target, link, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::createSymlink(const std::filesystem::path &target, const std::filesystem::path &link) {
    std::error_code ec;
    create_symlink(target, link, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::createDirectorySymlink(const std::filesystem::path &target, const std::filesystem::path &link) {
    std::error_code ec;
    create_directory_symlink(target, link, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::currentPath() {
    std::error_code ec;
    auto result = std::filesystem::current_path(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<void, std::error_code> zero::filesystem::currentPath(const std::filesystem::path &path) {
    std::error_code ec;
    current_path(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<bool, std::error_code> zero::filesystem::exists(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::exists(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code>
zero::filesystem::equivalent(const std::filesystem::path &p1, const std::filesystem::path &p2) {
    std::error_code ec;
    const auto result = std::filesystem::equivalent(p1, p2, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::uintmax_t, std::error_code> zero::filesystem::fileSize(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = file_size(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::uintmax_t, std::error_code> zero::filesystem::hardLinkCount(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = hard_link_count(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::file_time_type, std::error_code>
zero::filesystem::lastWriteTime(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = last_write_time(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<void, std::error_code>
zero::filesystem::lastWriteTime(const std::filesystem::path &path, const std::filesystem::file_time_type time) {
    std::error_code ec;
    last_write_time(path, time, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::permissions(
    const std::filesystem::path &path,
    const std::filesystem::perms perms,
    const std::filesystem::perm_options opts
) {
    std::error_code ec;
    std::filesystem::permissions(path, perms, opts, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::readSymlink(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = read_symlink(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::remove(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::remove(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::uintmax_t, std::error_code> zero::filesystem::removeAll(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = remove_all(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<void, std::error_code>
zero::filesystem::rename(const std::filesystem::path &from, const std::filesystem::path &to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::resizeFile(const std::filesystem::path &path, const std::uintmax_t size) {
    std::error_code ec;
    resize_file(path, size, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<std::filesystem::space_info, std::error_code> zero::filesystem::space(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::space(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::file_status, std::error_code>
zero::filesystem::status(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = std::filesystem::status(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::file_status, std::error_code>
zero::filesystem::symlinkStatus(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = symlink_status(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::path, std::error_code> zero::filesystem::temporaryDirectory() {
    std::error_code ec;
    auto result = std::filesystem::temp_directory_path(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isBlockFile(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_block_file(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isCharacterFile(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_character_file(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_directory(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isEmpty(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_empty(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isFIFO(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_fifo(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isOther(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_other(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isRegularFile(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_regular_file(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isSocket(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_socket(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::isSymlink(const std::filesystem::path &path) {
    std::error_code ec;
    const auto result = is_symlink(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

zero::filesystem::DirectoryEntry::DirectoryEntry(std::filesystem::directory_entry entry) : mEntry{std::move(entry)} {
}

tl::expected<void, std::error_code> zero::filesystem::DirectoryEntry::assign(const std::filesystem::path &path) {
    std::error_code ec;
    mEntry.assign(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code>
zero::filesystem::DirectoryEntry::replaceFilename(const std::filesystem::path &path) {
    std::error_code ec;
    mEntry.replace_filename(path, ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

tl::expected<void, std::error_code> zero::filesystem::DirectoryEntry::refresh() {
    std::error_code ec;
    mEntry.refresh(ec);

    if (ec)
        return tl::unexpected{ec};

    return {};
}

const std::filesystem::path &zero::filesystem::DirectoryEntry::path() const {
    return mEntry.path();
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::exists() const {
    std::error_code ec;
    const auto result = mEntry.exists(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isBlockFile() const {
    std::error_code ec;
    const auto result = mEntry.is_block_file(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isCharacterFile() const {
    std::error_code ec;
    const auto result = mEntry.is_character_file(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isDirectory() const {
    std::error_code ec;
    const auto result = mEntry.is_directory(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isFIFO() const {
    std::error_code ec;
    const auto result = mEntry.is_fifo(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isOther() const {
    std::error_code ec;
    const auto result = mEntry.is_other(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isRegularFile() const {
    std::error_code ec;
    const auto result = mEntry.is_regular_file(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isSocket() const {
    std::error_code ec;
    const auto result = mEntry.is_socket(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<bool, std::error_code> zero::filesystem::DirectoryEntry::isSymlink() const {
    std::error_code ec;
    const auto result = mEntry.is_symlink(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::uintmax_t, std::error_code> zero::filesystem::DirectoryEntry::fileSize() const {
    std::error_code ec;
    const auto result = mEntry.file_size(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::uintmax_t, std::error_code> zero::filesystem::DirectoryEntry::hardLinkCount() const {
    std::error_code ec;
    const auto result = mEntry.hard_link_count(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::file_time_type, std::error_code>
zero::filesystem::DirectoryEntry::lastWriteTime() const {
    std::error_code ec;
    const auto result = mEntry.last_write_time(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::file_status, std::error_code> zero::filesystem::DirectoryEntry::status() const {
    std::error_code ec;
    const auto result = mEntry.status(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

tl::expected<std::filesystem::file_status, std::error_code> zero::filesystem::DirectoryEntry::symlinkStatus() const {
    std::error_code ec;
    const auto result = mEntry.symlink_status(ec);

    if (ec)
        return tl::unexpected{ec};

    return result;
}

zero::filesystem::EntryProxy::EntryProxy(tl::expected<DirectoryEntry, std::error_code> entry)
    : mEntry{std::move(entry)} {
}

const tl::expected<zero::filesystem::DirectoryEntry, std::error_code> &
zero::filesystem::EntryProxy::operator*() const & {
    return mEntry;
}

tl::expected<zero::filesystem::DirectoryEntry, std::error_code> zero::filesystem::EntryProxy::operator*() && {
    return std::move(mEntry);
}

tl::expected<zero::filesystem::NoExcept<std::filesystem::directory_iterator>, std::error_code>
zero::filesystem::readDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::directory_iterator it{path, ec};

    if (ec)
        return tl::unexpected{ec};

    return NoExcept{std::move(it)};
}

tl::expected<zero::filesystem::NoExcept<std::filesystem::recursive_directory_iterator>, std::error_code>
zero::filesystem::walkDirectory(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it{path, ec};

    if (ec)
        return tl::unexpected{ec};

    return NoExcept{std::move(it)};
}
