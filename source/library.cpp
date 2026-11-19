#include "library.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>

#include "fsx.h"
#include "sorter.h"

namespace library {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool isGameExt(const std::string& ext) { return ext == "iso" || ext == "bin" || ext == "img" || ext == "mdf"; }

// "(Track 02)" and above belong to a game that is already listed through "(Track 01)".
bool isExtraTrack(const std::string& name) {
    const std::string l = lower(name);
    const auto p = l.find("(track ");
    if (p == std::string::npos) return false;
    const int n = std::atoi(l.c_str() + p + 7);
    return n > 1;
}

}  // namespace

const char* label(Platform p) {
    switch (p) {
        case Platform::Ps3: return "PS3";
        case Platform::Ps2: return "PS2";
        case Platform::Psx: return "PS1";
    }
    return "";
}

std::vector<Game> scan(const std::vector<Root>& roots) {
    std::vector<Game> games;
    for (const Root& root : roots) {
        const std::vector<std::string> names = fsx::listDir(root.path);
        std::map<std::string, std::string> byLower;  // lowercase file name -> real file name
        for (const std::string& n : names) byLower[lower(n)] = n;

        for (const std::string& file : names) {
            if (file.compare(0, 2, "._") == 0) continue;
            const auto dot = file.rfind('.');
            if (dot == std::string::npos || dot == 0) continue;
            if (!isGameExt(lower(file.substr(dot + 1)))) continue;
            const std::string name = file.substr(0, dot);
            if (isExtraTrack(name)) continue;

            Game g;
            g.name = name;
            g.title = sorter::prettyTitle(name);
            g.platform = root.platform;
            g.dir = root.path;
            for (const std::string& stem : {name, file}) {
                for (const char* ext : {".png", ".jpg", ".jpeg"}) {
                    const auto it = byLower.find(lower(stem + ext));
                    if (it != byLower.end() && g.image.empty()) g.image = root.path + "/" + it->second;
                }
            }
            games.push_back(std::move(g));
        }
    }
    return games;
}

}  // namespace library
