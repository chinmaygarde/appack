#include "hasher.h"

#include <blake3.h>
#include <sstream>

namespace pack {

ContentHash GetMappingHash(const Mapping& mapping) {
  blake3_hasher hasher = {};
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, mapping.GetData(), mapping.GetSize());
  ContentHash hash;
  static_assert(hash.size() == BLAKE3_OUT_LEN);
  blake3_hasher_finalize(&hasher, hash.data(), hash.size());
  return hash;
}

std::optional<ContentHash> ParseFromHexString(const std::string_view& str) {
  ContentHash hash;
  if (str.size() != hash.size() * 2u) {
    return std::nullopt;
  }
  for (size_t i = 0; i < hash.size(); i++) {
    hash[i] = std::stoi(std::string{str.substr(i * 2, 2)}, nullptr, 16);
  }
  return hash;
}

std::string ToString(const ContentHash& hash) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (uint8_t b : hash) {
    stream << std::setw(2) << static_cast<int>(b);
  }
  return stream.str();
}

}  // namespace pack
