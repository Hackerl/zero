#ifndef ZERO_SHELL_H
#define ZERO_SHELL_H

#include <string>
#include <list>

namespace zero {
    namespace sh {
        bool match(const std::string &pattern, std::list<std::string> &paths);
        bool expansion(const std::string &str, std::list<std::string> &words);
    }
}

#endif //ZERO_SHELL_H
