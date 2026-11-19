// Puts a downloaded cover in the same frame the game list uses: a square transparent PNG with the picture
// shrunk to fit, aspect kept and centered. Box art from the repositories is large (up to 1000+ pixels and
// close to 1 MB); the XMB does not show those, and the covers already on the console are 160 x 160.
#pragma once

#include <cstdint>
#include <vector>

namespace frame {

constexpr int kSize = 160;

/// Decodes a PNG or JPEG and writes it as a `size` x `size` PNG. A picture that already fits is not enlarged.
///
/// Args:
///     image: The downloaded file.
///     size: Side of the square frame.
///     png: Receives the PNG file.
///
/// Returns:
///     False when the file is not a PNG or JPEG of a usable size.
bool fit(const std::vector<std::uint8_t>& image, int size, std::vector<std::uint8_t>& png);

}  // namespace frame
