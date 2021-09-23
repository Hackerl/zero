#include <zero/filesystem/directory.h>
#include <zero/filesystem/path.h>
#include <fcntl.h>
#include <unistd.h>

zero::filesystem::CDirectoryIterator::CDirectoryIterator(zero::filesystem::CDirectoryIterator &iterator) noexcept {
    mEntry = iterator.mEntry;
    mContexts = iterator.clone();
}

zero::filesystem::CDirectoryIterator::CDirectoryIterator(zero::filesystem::CDirectoryIterator &&iterator) noexcept {
    mEntry = std::move(iterator.mEntry);
    mContexts = std::move(iterator.mContexts);
}

zero::filesystem::CDirectoryIterator::CDirectoryIterator(const std::string &path, unsigned int deep) {
    DIR *dir = opendir(path.c_str());

    if (dir) {
        mContexts.emplace_back(dir, deep);
        operator++();
    }
}

zero::filesystem::CDirectoryIterator::~CDirectoryIterator() {
    for (const auto &context : mContexts) {
        closedir(std::get<0>(context));
    }
}

std::list<std::tuple<DIR *, unsigned int>> zero::filesystem::CDirectoryIterator::clone() const {
    std::list<std::tuple<DIR *, unsigned int>> contexts;

    for (const auto &context : mContexts) {
        DIR *dir = std::get<0>(context);
        unsigned int deep = std::get<1>(context);

        DIR *newDir = opendir(path::getFileDescriptorPath(dirfd(dir)).c_str());

        if (!newDir)
            continue;

        long position = telldir(dir);

        if (position < 0)
            continue;

        seekdir(newDir, position);
        contexts.emplace_back(newDir, deep);
    }

    return contexts;
}

zero::filesystem::CDirectoryIterator &zero::filesystem::CDirectoryIterator::operator++() {
    while (!mContexts.empty()) {
        std::tuple<DIR *, unsigned int> context = mContexts.back();

        DIR *dir = std::get<0>(context);
        unsigned int deep = std::get<1>(context);

        dirent *entry = readdir(dir);

        if (entry) {
            std::string name = entry->d_name;
            bool isDirectory = entry->d_type == DT_DIR;

            if (name == "." || name == "..")
                continue;

            if (isDirectory && deep > 1) {
                int fd = openat(dirfd(dir), name.c_str(), O_RDONLY | O_DIRECTORY);

                if (fd < 0)
                    continue;

                DIR *subdir = fdopendir(fd);

                if (!subdir) {
                    close(fd);
                    continue;
                }

                mContexts.emplace_back(subdir, deep - 1);
            }

            mEntry = {
                    path::join(
                            path::getFileDescriptorPath(dirfd(dir)),
                            name
                    ),
                    isDirectory
            };

            break;
        }

        closedir(dir);
        mContexts.pop_back();
    }

    return *this;
}

zero::filesystem::CDirectoryIterator &zero::filesystem::CDirectoryIterator::operator=(const zero::filesystem::CDirectoryIterator &iterator) {
    for (const auto &context : mContexts) {
        closedir(std::get<0>(context));
    }

    mEntry = iterator.mEntry;
    mContexts = iterator.clone();

    return *this;
}

zero::filesystem::CDirectoryEntry &zero::filesystem::CDirectoryIterator::operator*() {
    return mEntry;
}

bool zero::filesystem::CDirectoryIterator::operator!=(const zero::filesystem::CDirectoryIterator &iterator) const {
    return !operator==(iterator);
}

bool zero::filesystem::CDirectoryIterator::operator==(const zero::filesystem::CDirectoryIterator &iterator) const {
    if (mContexts.empty() && iterator.mContexts.empty())
        return true;

    return mEntry.path == iterator.mEntry.path && mEntry.isDirectory == iterator.mEntry.isDirectory;
}

zero::filesystem::CDirectoryIterator zero::filesystem::begin(const zero::filesystem::CDirectory &directory) {
    return CDirectoryIterator(directory.path, directory.deep);
}

zero::filesystem::CDirectoryIterator zero::filesystem::end(const zero::filesystem::CDirectory &directory) {
    return {};
}
