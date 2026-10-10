#include "font_files.h"

#include <stdlib.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <dirent.h>

#include "port/native/platform.h"
#else
#include <windows.h>
#endif

#include "../config/config.h"
#include "../res/path.h"

namespace mwtext {

namespace {

std::vector<std::string>& ExtraDirs() {
    static std::vector<std::string> dirs;
    return dirs;
}

char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

bool EqualNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}

bool IsFile(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}

const char* Env(const char* name) {
    const char* v = getenv(name);
    return (v && *v) ? v : nullptr;
}

std::vector<std::string> SystemDirectories() {
    std::vector<std::string> dirs;
#if defined(_WIN32)
    if (const char* windir = Env("WINDIR")) dirs.push_back(std::string(windir) + "\\Fonts");
#else
    dirs = reb::platform::SystemFontDirs();
#endif
    return dirs;
}

std::string BoldSibling(const std::string& regular) {
    size_t slash = regular.find_last_of("/\\");
    std::string dir = slash == std::string::npos ? std::string() : regular.substr(0, slash + 1);
    std::string name = slash == std::string::npos ? regular : regular.substr(slash + 1);
    std::string lower;
    for (char c : name) lower.push_back(Lower(c));
    std::string candidate;
    if (lower == "arial.ttf") candidate = "arialbd.ttf";
    else if (lower.size() > 12 && lower.compare(lower.size() - 12, 12, "-regular.ttf") == 0)
        candidate = name.substr(0, name.size() - 12) + "-Bold.ttf";
    else if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".ttf") == 0)
        candidate = name.substr(0, name.size() - 4) + "bd.ttf";
    if (candidate.empty()) return std::string();
    std::string found = FindFileNoCase(dir.empty() ? "." : dir, candidate, 0);
    return found;
}

}  // namespace

void SetExtraFontDirectories(const std::vector<std::string>& dirs) { ExtraDirs() = dirs; }

std::string FindFileNoCase(const std::string& directory, const std::string& fileName, int maxDepth) {
#if defined(_WIN32)
    WIN32_FIND_DATAA data;
    std::string pattern = directory + "\\*";
    HANDLE h = FindFirstFileA(pattern.c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) return std::string();
    std::string result;
    std::vector<std::string> subdirs;
    do {
        std::string n = data.cFileName;
        if (n == "." || n == "..") continue;
        std::string full = directory + "\\" + n;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) subdirs.push_back(full);
        else if (EqualNoCase(n, fileName)) {
            result = full;
            break;
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);
    if (!result.empty() || maxDepth <= 0) return result;
    for (const std::string& d : subdirs) {
        std::string r = FindFileNoCase(d, fileName, maxDepth - 1);
        if (!r.empty()) return r;
    }
    return std::string();
#else
    DIR* d = opendir(directory.c_str());
    if (!d) return std::string();
    std::vector<std::string> subdirs;
    std::string result;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string full = directory + "/" + n;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) continue;
        if ((st.st_mode & S_IFMT) == S_IFDIR) subdirs.push_back(full);
        else if (EqualNoCase(n, fileName)) {
            result = full;
            break;
        }
    }
    closedir(d);
    if (!result.empty() || maxDepth <= 0) return result;
    for (const std::string& sub : subdirs) {
        std::string r = FindFileNoCase(sub, fileName, maxDepth - 1);
        if (!r.empty()) return r;
    }
    return std::string();
#endif
}

namespace {

std::string SearchAll(const std::vector<std::string>& dirs, const std::string& name) {
    for (const std::string& d : dirs) {
        std::string f = FindFileNoCase(d, name);
        if (!f.empty()) return f;
    }
    return std::string();
}

}  // namespace

FontFiles FindArial() {
    FontFiles files;
    std::string configured = mw::config::Global().FontPath();
    if (!configured.empty()) {
        std::string path = mw::res::ResolvePath(configured);
        if (IsFile(path)) {
            files.regular = path;
            files.bold = BoldSibling(path);
            files.source = "config";
            return files;
        }
    }
    std::vector<std::string> dirs = ExtraDirs();
    if (const char* envDir = Env("MW_FONT_DIR")) dirs.push_back(envDir);
    for (const std::string& d : SystemDirectories()) dirs.push_back(d);

    std::string arial = SearchAll(dirs, "arial.ttf");
    if (!arial.empty()) {
        files.regular = arial;
        files.bold = BoldSibling(arial);
        files.source = "system Arial";
        return files;
    }
    std::vector<std::string> libDirs = dirs;
    if (const char* exe = Env("MW_EXE_DIR")) libDirs.push_back(std::string(exe) + "/fonts");
#if !defined(_WIN32)
    libDirs.push_back(reb::platform::BundledFontDir());
#endif
    libDirs.push_back("third_party/liberation-fonts-ttf-2.1.5");
    libDirs.push_back("../third_party/liberation-fonts-ttf-2.1.5");
    std::string lib = SearchAll(libDirs, "LiberationSans-Regular.ttf");
    if (!lib.empty()) {
        files.regular = lib;
        files.bold = BoldSibling(lib);
        files.source = "Liberation Sans";
    }
    return files;
}

}  // namespace mwtext
