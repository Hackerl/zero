#ifndef ZERO_PATH_H
#define ZERO_PATH_H

#include <string>

namespace zero {
    namespace filesystem {
        namespace path {
#ifdef _WIN32
            constexpr auto PATH_SEPARATOR = '\\';
#else
            constexpr auto PATH_SEPARATOR = '/';
#endif

            std::string getFileDescriptorPath(int fd);
            std::string getApplicationDirectory();
            std::string getApplicationPath();
            std::string getApplicationName();
            std::string getAbsolutePath(const std::string &path);

            bool isDirectory(const std::string &path);
            bool isRegularFile(const std::string &path);

            std::string join(const std::string &path);

            template<typename... Args>
            std::string join(const std::string &path, Args... args) {
                if (path.empty())
                    return join(args...);

                if (path.back() != PATH_SEPARATOR) {
                    return path + PATH_SEPARATOR + join(args...);
                } else {
                    return path + join(args...);
                }
            }
        }
    }
}


#endif //ZERO_PATH_H
