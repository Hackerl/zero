#ifndef ZERO_DIRECTORY_H
#define ZERO_DIRECTORY_H

#include <string>
#include <list>
#include <tuple>

#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <dirent.h>
#endif

namespace zero {
    namespace filesystem {
        struct CDirectoryEntry {
            std::string path;
            bool isDirectory;
        };

        class CDirectoryIterator {
        public:
            CDirectoryIterator() = default;
            CDirectoryIterator(CDirectoryIterator &&iterator) noexcept;
            explicit CDirectoryIterator(const std::string &path, unsigned int deep);
            ~CDirectoryIterator();

#ifdef _WIN32
            CDirectoryIterator(CDirectoryIterator &iterator) = delete;
#elif __linux__
            CDirectoryIterator(CDirectoryIterator &iterator) noexcept;
#endif

        public:
            CDirectoryIterator &operator++();
            CDirectoryEntry &operator*();
#ifdef __linux__
            CDirectoryIterator &operator=(const CDirectoryIterator &iterator);
#endif

        public:
            bool operator!=(const CDirectoryIterator &iterator) const;
            bool operator==(const CDirectoryIterator &iterator) const;

        private:
#ifdef __linux__
            std::list<std::tuple<DIR *, unsigned int>> clone() const;
#endif

        private:
            CDirectoryEntry mEntry{};

#ifdef _WIN32
            std::list<std::tuple<HANDLE, std::string, unsigned int>> mContexts;
#elif __linux__
            std::list<std::tuple<DIR *, unsigned int>> mContexts;
#endif
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
