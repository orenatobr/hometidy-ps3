// "Completed" badge drawn straight into a cover image (PNG or JPEG).
//
// The look matches the badge of the ps3tweaks FTP cover marker (green rounded square with a white
// check in the top-left corner, about 15% of the cover), so covers marked by either tool are
// recognized by both.
#pragma once

#include <cstdint>
#include <vector>

namespace badge {

enum class Format { Unknown, Png, Jpeg };

/// Top-down RGBA, 8 bits per channel, straight alpha.
struct Image {
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgba;
};

constexpr int kMinSize = 32;
constexpr int kMaxSize = 4096;

/// Looks at the first bytes of a file.
Format sniff(const std::vector<std::uint8_t>& file);

/// Decodes a PNG or JPEG cover. False when the format is unsupported or the size is outside 32..4096.
bool decode(const std::vector<std::uint8_t>& file, Image& out);

/// Encodes the image as `format` (JPEG at quality 95). False for Format::Unknown.
bool encode(const Image& image, Format format, std::vector<std::uint8_t>& out);

/// The badge square: top-left corner and side (the square spans x..x+side inclusive).
struct Box {
    int x = 0, y = 0, side = 0;
};

/// Badge position for an image of the given size.
Box boxFor(int width, int height);

/// Draws the badge in the top-left corner, in place.
void apply(Image& image);

/// Removes the badge without a saved original, in place.
///
/// The badge covered the pixels under it, so they are rebuilt from the pixels just outside it
/// (an approximation; a saved original is always better).
void erase(Image& image);

/// True when the badge colours are present where apply() paints them.
bool isMarked(const Image& image);

}  // namespace badge
