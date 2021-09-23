#ifndef ZERO_DIRECTORY_H
#define ZERO_DIRECTORY_H

#include <string>
#include <list>
#include <dirent.h>
#include <tuple>

namespace zero {
    namespace filesystem {
        struct CDirectoryEntry {
            std::string path;
            bool isDirectory;
        };

        class CDirectoryIterator {
        public:
            CDirectoryIterator() = default;
            CDirectoryIterator(CDirectoryIterator &iterator) noexcept;
            CDirectoryIterator(CDirectoryIterator &&iterator) noexcept;
            explicit CDirectoryIterator(const std::string &path, unsigned int deep);
            ~CDirectoryIterator();

        public:
            CDirectoryIterator &operator++();
            CDirectoryIterator &operator=(const CDirectoryIterator &iterator);
            CDirectoryEntry &operator*();

        public:
            bool operator!=(const CDirectoryIterator &iterator) const;
            bool operator==(const CDirectoryIterator &iterator) const;

        private:
            std::list<std::tuple<DIR *, unsigned int>> clone() const;

        private:
            CDirectoryEntry mEntry{};
            std::list<std::tuple<DIR *, unsigned int>> mContexts;
        };

        struct CDirectory {
            std::string path;
            unsigned int deep;
        };

        CDirectoryIterator begin(const CDirectory &directory);
        CDirectoryIterator end(const CDirectory &directory);
    }
}

#endif //ZERO_DIRECTORY_H
