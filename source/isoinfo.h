// Reads the id of a game out of its disc image, so a cover can be looked up by title id.
#pragma once

#include <string>

namespace isoinfo {

/// Title id of a PS3 game image ("BLUS30341"), from PS3_GAME/PARAM.SFO. Empty when the image cannot be read.
std::string ps3TitleId(const std::string& isoPath);

}  // namespace isoinfo
