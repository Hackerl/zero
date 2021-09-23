#include <zero/sh/shell.h>
#include <glob.h>
#include <wordexp.h>

bool zero::sh::match(const std::string &pattern, std::list<std::string> &paths) {
    glob_t g = {};

    if (glob(pattern.c_str(), 0, nullptr, &g) != 0) {
        globfree(&g);
        return false;
    }

    for (int i = 0; i < g.gl_pathc; i++) {
        paths.emplace_back(g.gl_pathv[i]);
    }

    globfree(&g);

    return true;
}

bool zero::sh::expansion(const std::string &str, std::list<std::string> &words) {
    wordexp_t p = {};

    if (wordexp(str.c_str(), &p, WRDE_NOCMD) != 0) {
        wordfree(&p);
        return false;
    }

    for (int i = 0; i < p.we_wordc; i++) {
        words.emplace_back(p.we_wordv[i]);
    }

    wordfree(&p);

    return true;
}
