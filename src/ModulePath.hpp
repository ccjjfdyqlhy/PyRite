#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

// ============================================================================
// Module paths
// ============================================================================
//
// A require names a module.  This is where a name turns into a file.
//
// The order is the same for the compiler and the interpreter, so a story
// that runs on this machine runs in the browser too:
//
//   1. the directory of the file that asks for it (so a module can sit
//      next to the story that uses it),
//   2. PYRITE_LIB_PATH (a list, separated by colons),
//   3. the lib directory next to the compiler,
//   4. ~/PyRite/lib, where the library that ships with the language lives.
//
// A directory is a module too: its _index.pr is the module.

namespace ModulePath {

inline bool is_file(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

inline bool is_dir(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

inline std::string dir_of(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

// The directory the executable lives in.
inline std::string executable_dir() {
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) return ".";
    exe_path[len] = '\0';
    return dir_of(exe_path);
}

// Every place a module may live, in the order they are tried.
inline std::vector<std::string> search_paths(const std::string& relative_to) {
    std::vector<std::string> paths;
    if (!relative_to.empty()) paths.push_back(relative_to);
    const char* env = std::getenv("PYRITE_LIB_PATH");
    if (env && *env) {
        std::string list(env);
        size_t start = 0;
        while (true) {
            size_t stop = list.find(':', start);
            std::string piece = (stop == std::string::npos)
                ? list.substr(start)
                : list.substr(start, stop - start);
            if (!piece.empty()) paths.push_back(piece);
            if (stop == std::string::npos) break;
            start = stop + 1;
        }
    }
    // The library that ships with the language.  The modules live in
    // <root>/lib, so "list" finds <root>/lib/list.pr; the root itself
    // is on the list too, so a story can also say "lib/list" and
    // "lib" (the umbrella module).
    std::string exe_root = executable_dir();
    paths.push_back(exe_root + "/lib");
    paths.push_back(exe_root);
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::string root = std::string(home) + "/PyRite";
        paths.push_back(root + "/lib");
        paths.push_back(root);
    }
    // The place the program was started from.  It is the last one, so a
    // module of the project always wins over one of the library.
    paths.push_back(".");
    return paths;
}

// Turn a module name into a file.  Returns false when there is no such
// module; 'tried' then lists where it looked.
inline bool resolve(const std::string& path, const std::string& relative_to,
                    std::string& resolved, bool& is_dir_module,
                    std::string& tried) {
    is_dir_module = false;
    tried.clear();
    std::vector<std::string> bases;
    bool absolute = !path.empty() && (path[0] == '/' ||
                                      (path.size() > 1 && path[1] == ':'));
    if (absolute) {
        bases.push_back("");
    } else {
        bases = search_paths(relative_to);
    }
    for (const auto& base : bases) {
        std::string candidate = base.empty() ? path : (base + "/" + path);
        if (is_file(candidate)) { resolved = candidate; return true; }
        if (is_dir(candidate)) {
            std::string index = candidate + "/_index.pr";
            if (is_file(index)) {
                resolved = index;
                is_dir_module = true;
                return true;
            }
        }
        if (is_file(candidate + ".pr")) {
            resolved = candidate + ".pr";
            return true;
        }
        if (tried.find(candidate) == std::string::npos) {
            if (!tried.empty()) tried += ", ";
            tried += candidate;
        }
    }
    return false;
}

} // namespace ModulePath
