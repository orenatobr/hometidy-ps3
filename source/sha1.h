// SHA-1, enough to compare a local file with the blob ids that git (and so GitHub) uses.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sha1 {

/// Hex SHA-1 of the bytes.
std::string hex(const std::uint8_t* data, std::size_t size);

/// Hex git blob id of the bytes: SHA-1 of "blob <size>\0" followed by the content.
/// This is the "sha" GitHub reports for a file in a tree.
std::string gitBlob(const std::vector<std::uint8_t>& data);

}  // namespace sha1
